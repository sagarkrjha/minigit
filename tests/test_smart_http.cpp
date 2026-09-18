#include "test_framework.h"
#include "remotes/pkt_line.h"
#include "remotes/http_client.h"
#include "remotes/pack_unpack.h"
#include "remotes/smart_http.h"
#include "remotes/config.h"
#include "storage/object_database.h"
#include "storage/blob.h"
#include "storage/tree.h"
#include "storage/commit.h"
#include "storage/pack.h"
#include "storage/object_parser.h"
#include "staging/index.h"
#include "repository/repository.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_handle_t = SOCKET;
constexpr socket_handle_t INVALID_SOCK = INVALID_SOCKET;
inline void close_sock(socket_handle_t s) { closesocket(s); }
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using socket_handle_t = int;
constexpr socket_handle_t INVALID_SOCK = -1;
inline void close_sock(socket_handle_t s) { close(s); }
#endif

namespace fs = std::filesystem;
using namespace minigit::remotes;

namespace {

fs::path make_temp_dir(const std::string &prefix)
{
    const auto p = fs::temp_directory_path() /
        ("minigit_http_test_" + prefix + "_" +
         std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    fs::create_directories(p);
    return p;
}

void remove_temp_dir(const fs::path &p)
{
    std::error_code ec;
    fs::remove_all(p, ec);
}

std::string read_file_text(const fs::path &p)
{
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string s = ss.str();
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

void write_file_text(const fs::path &p, const std::string &c)
{
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(c.data(), static_cast<std::streamsize>(c.size()));
}

// ---------------------------------------------------------------------------
// Embedded Mock HTTP Server for Git Smart HTTP Protocol
// ---------------------------------------------------------------------------

class MockGitHttpServer
{
public:
    explicit MockGitHttpServer(fs::path repo_dir)
        : repo_dir_(std::move(repo_dir)), running_(true)
    {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        server_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_sock_ == INVALID_SOCK)
            throw std::runtime_error("Failed to create server socket");

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0; // Ephemeral port

        if (bind(server_sock_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
        {
            close_sock(server_sock_);
            throw std::runtime_error("Failed to bind server socket");
        }

        socklen_t len = sizeof(addr);
        if (getsockname(server_sock_, reinterpret_cast<sockaddr *>(&addr), &len) != 0)
        {
            close_sock(server_sock_);
            throw std::runtime_error("Failed to get socket name");
        }
        port_ = ntohs(addr.sin_port);

        if (listen(server_sock_, 16) != 0)
        {
            close_sock(server_sock_);
            throw std::runtime_error("Failed to listen on server socket");
        }

        worker_ = std::thread(&MockGitHttpServer::server_loop, this);
    }

    ~MockGitHttpServer()
    {
        running_ = false;
        // Connect to wake up accept
        socket_handle_t wake_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (wake_sock != INVALID_SOCK)
        {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(port_);
            connect(wake_sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
            close_sock(wake_sock);
        }

        if (worker_.joinable())
            worker_.join();

        if (server_sock_ != INVALID_SOCK)
            close_sock(server_sock_);
    }

    uint16_t port() const { return port_; }
    std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port_) + "/test.git"; }

private:
    void server_loop()
    {
        while (running_)
        {
            sockaddr_in client_addr{};
            socklen_t len = sizeof(client_addr);
            socket_handle_t client = accept(server_sock_, reinterpret_cast<sockaddr *>(&client_addr), &len);
            if (client == INVALID_SOCK)
                break;

            if (!running_)
            {
                close_sock(client);
                break;
            }

            handle_client(client);
            close_sock(client);
        }
    }

    void handle_client(socket_handle_t client)
    {
        std::string req;
        char buf[4096];
        size_t header_end = std::string::npos;

        while (true)
        {
            int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0) break;
            req.append(buf, n);
            header_end = req.find("\r\n\r\n");
            if (header_end != std::string::npos) break;
        }

        if (header_end == std::string::npos) return;

        // Content-Length check
        size_t body_start = header_end + 4;
        size_t cl_pos = req.find("Content-Length: ");
        if (cl_pos == std::string::npos) cl_pos = req.find("content-length: ");
        if (cl_pos != std::string::npos && cl_pos < header_end)
        {
            size_t cl_end = req.find("\r\n", cl_pos);
            size_t content_len = std::stoul(req.substr(cl_pos + 16, cl_end - (cl_pos + 16)));
            while (req.size() - body_start < content_len)
            {
                int n = recv(client, buf, sizeof(buf), 0);
                if (n <= 0) break;
                req.append(buf, n);
            }
        }

        std::string request_line;
        {
            size_t eol = req.find("\r\n");
            if (eol != std::string::npos)
                request_line = req.substr(0, eol);
        }

        std::string method, path;
        {
            std::istringstream iss(request_line);
            iss >> method >> path;
        }

        std::string body = (req.size() > body_start) ? req.substr(body_start) : "";

        // Route requests
        if (method == "GET" && path.find("/info/refs") != std::string::npos)
        {
            if (path.find("service=git-upload-pack") != std::string::npos)
            {
                handle_info_refs_upload_pack(client);
            }
            else if (path.find("service=git-receive-pack") != std::string::npos)
            {
                handle_info_refs_receive_pack(client);
            }
            else
            {
                send_response(client, 400, "Bad Request", "text/plain", "Unknown service");
            }
        }
        else if (method == "POST" && path.find("/git-upload-pack") != std::string::npos)
        {
            handle_git_upload_pack(client, body);
        }
        else if (method == "POST" && path.find("/git-receive-pack") != std::string::npos)
        {
            handle_git_receive_pack(client, body);
        }
        else
        {
            send_response(client, 404, "Not Found", "text/plain", "Not Found");
        }
    }

    void handle_info_refs_upload_pack(socket_handle_t client)
    {
        fs::path main_ref_path = repo_dir_ / ".minigit" / "refs" / "heads" / "main";
        std::string main_sha = read_file_text(main_ref_path);

        std::string payload;
        payload += pkt_line("# service=git-upload-pack\n");
        payload += pkt_flush();

        if (!main_sha.empty())
        {
            using namespace std::string_literals;
            std::string line = main_sha + " HEAD\0symref=HEAD:refs/heads/main ofs-delta agent=minigit/1.8.1\n"s;
            payload += pkt_line(line);
            payload += pkt_line(main_sha + " refs/heads/main\n");
        }
        payload += pkt_flush();

        send_response(client, 200, "OK", "application/x-git-upload-pack-advertisement", payload);
    }

    void handle_info_refs_receive_pack(socket_handle_t client)
    {
        fs::path main_ref_path = repo_dir_ / ".minigit" / "refs" / "heads" / "main";
        std::string main_sha = read_file_text(main_ref_path);

        std::string payload;
        payload += pkt_line("# service=git-receive-pack\n");
        payload += pkt_flush();

        if (!main_sha.empty())
        {
            using namespace std::string_literals;
            std::string line = main_sha + " refs/heads/main\0report-status agent=minigit/1.8.1\n"s;
            payload += pkt_line(line);
        }
        payload += pkt_flush();

        send_response(client, 200, "OK", "application/x-git-receive-pack-advertisement", payload);
    }

    void handle_git_upload_pack(socket_handle_t client, const std::string &body)
    {
        (void)body;
        // Pack all objects in the repository
        fs::path objects_dir = repo_dir_ / ".minigit" / "objects";
        ObjectDatabase db(objects_dir);

        // Gather all loose objects
        std::vector<minigit::storage::PackEntry> entries;
        if (fs::exists(objects_dir))
        {
            for (const auto &p1 : fs::directory_iterator(objects_dir))
            {
                if (!p1.is_directory()) continue;
                std::string dname = p1.path().filename().string();
                if (dname.size() != 2 || dname == "pack" || dname == "info") continue;

                for (const auto &p2 : fs::directory_iterator(p1.path()))
                {
                    if (!p2.is_regular_file()) continue;
                    std::string sha = dname + p2.path().filename().string();
                    try
                    {
                        std::string envelope = db.read(sha);
                        size_t null_pos = envelope.find('\0');
                        std::string header = envelope.substr(0, null_pos);
                        std::string type_name = header.substr(0, header.find(' '));

                        minigit::storage::PackEntry pe;
                        pe.id = sha;
                        pe.type_name = type_name;
                        if (type_name == "commit") pe.type_num = minigit::storage::OBJ_COMMIT;
                        else if (type_name == "tree") pe.type_num = minigit::storage::OBJ_TREE;
                        else if (type_name == "blob") pe.type_num = minigit::storage::OBJ_BLOB;
                        else pe.type_num = minigit::storage::OBJ_TAG;

                        pe.payload = envelope.substr(null_pos + 1);
                        entries.push_back(std::move(pe));
                    }
                    catch (...) {}
                }
            }
        }

        // Generate packfile
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        fs::path temp_pack_dir = objects_dir / ("temp_server_pack_" + std::to_string(now));
        fs::create_directories(temp_pack_dir);

        minigit::storage::PackWriteResult pwrite = minigit::storage::write_pack(temp_pack_dir, entries);
        std::ifstream pf(pwrite.pack_path, std::ios::binary);
        std::string pack_bytes((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
        pf.close();
        fs::remove_all(temp_pack_dir);

        // Response format: NAK\n + pack_bytes
        std::string resp_body = pkt_line("NAK\n");
        resp_body.append(pack_bytes);

        send_response(client, 200, "OK", "application/x-git-upload-pack-result", resp_body);
    }

    void handle_git_receive_pack(socket_handle_t client, const std::string &body)
    {
        // Request format: pkt-line commands + pkt_flush + packfile
        size_t offset = 0;
        std::string target_ref;
        std::string target_new_sha;

        while (offset < body.size())
        {
            PktLineResult line = read_pkt_line(body, offset);
            if (line.is_flush || line.is_eof) break;

            std::string text = line.payload;
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
                text.pop_back();

            size_t null_pos = text.find('\0');
            if (null_pos != std::string::npos)
                text = text.substr(0, null_pos);

            std::istringstream iss(text);
            std::string old_sha, new_sha, ref_name;
            if (iss >> old_sha >> new_sha >> ref_name)
            {
                target_ref = ref_name;
                target_new_sha = new_sha;
            }
        }

        // The remaining bytes starting at offset contain the packfile
        if (offset < body.size())
        {
            std::string pack_data = body.substr(offset);
            unpack_pack_stream_to_db(pack_data, repo_dir_ / ".minigit" / "objects");
        }

        if (!target_ref.empty() && !target_new_sha.empty())
        {
            write_file_text(repo_dir_ / ".minigit" / target_ref, target_new_sha + "\n");
        }

        std::string resp_body;
        resp_body += pkt_line("unpack ok\n");
        if (!target_ref.empty())
            resp_body += pkt_line("ok " + target_ref + "\n");
        resp_body += pkt_flush();

        send_response(client, 200, "OK", "application/x-git-receive-pack-result", resp_body);
    }

    void send_response(
        socket_handle_t client,
        int code,
        const std::string &status,
        const std::string &content_type,
        const std::string &body)
    {
        std::string resp = "HTTP/1.1 " + std::to_string(code) + " " + status + "\r\n";
        resp += "Content-Type: " + content_type + "\r\n";
        resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        resp += "Connection: close\r\n\r\n";
        resp += body;

        size_t sent = 0;
        while (sent < resp.size())
        {
            int n = send(client, resp.data() + sent, static_cast<int>(resp.size() - sent), 0);
            if (n <= 0) break;
            sent += n;
        }
    }

    fs::path repo_dir_;
    std::atomic<bool> running_{false};
    socket_handle_t server_sock_{INVALID_SOCK};
    uint16_t port_{0};
    std::thread worker_;
};

} // namespace

// ---------------------------------------------------------------------------
// Unit Tests for pkt-line framing and parsing
// ---------------------------------------------------------------------------

TEST_CASE(SmartHttp, PktLineFraming)
{
    ASSERT_EQ(pkt_line("hello\n"), "000ahello\n");
    ASSERT_EQ(pkt_line(""), "0004");
    ASSERT_EQ(pkt_flush(), "0000");
    ASSERT_EQ(pkt_delim(), "0001");

    std::string stream = pkt_line("want 123\n") + pkt_delim() + pkt_line("have 456\n") + pkt_flush();
    size_t offset = 0;

    PktLineResult p1 = read_pkt_line(stream, offset);
    ASSERT_FALSE(p1.is_flush);
    ASSERT_FALSE(p1.is_delim);
    ASSERT_FALSE(p1.is_eof);
    ASSERT_EQ(p1.payload, "want 123\n");

    PktLineResult p2 = read_pkt_line(stream, offset);
    ASSERT_TRUE(p2.is_delim);
    ASSERT_FALSE(p2.is_flush);

    PktLineResult p3 = read_pkt_line(stream, offset);
    ASSERT_FALSE(p3.is_flush);
    ASSERT_FALSE(p3.is_delim);
    ASSERT_EQ(p3.payload, "have 456\n");

    PktLineResult p4 = read_pkt_line(stream, offset);
    ASSERT_TRUE(p4.is_flush);

    PktLineResult p5 = read_pkt_line(stream, offset);
    ASSERT_TRUE(p5.is_eof);
}

TEST_CASE(SmartHttp, AdvertisedRefsParsing)
{
    using namespace std::string_literals;
    std::string valid_response =
        pkt_line("# service=git-upload-pack\n") +
        pkt_flush() +
        pkt_line("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890 HEAD\0symref=HEAD:refs/heads/main ofs-delta agent=minigit/1.8.1\n"s) +
        pkt_line("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890 refs/heads/main\n") +
        pkt_line("1111111111111111111111111111111111111111111111111111111111111111 refs/tags/v1.0\n") +
        pkt_flush();

    AdvertisedRefsResult res = parse_advertised_refs(valid_response, "git-upload-pack");
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.service, "git-upload-pack");
    ASSERT_EQ(res.symref_head, "refs/heads/main");
    ASSERT_EQ(res.refs.size(), 3);
    ASSERT_EQ(res.ref_map["HEAD"], "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890");
    ASSERT_EQ(res.ref_map["refs/heads/main"], "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890");
    ASSERT_EQ(res.ref_map["refs/tags/v1.0"], "1111111111111111111111111111111111111111111111111111111111111111");

    // Invalid service header test
    std::string invalid_response = pkt_line("# service=git-other-service\n") + pkt_flush();
    AdvertisedRefsResult bad_res = parse_advertised_refs(invalid_response, "git-upload-pack");
    ASSERT_FALSE(bad_res.success);
    ASSERT_FALSE(bad_res.error.empty());
}

// ---------------------------------------------------------------------------
// Unit Tests for Pack Extraction and Unpacking
// ---------------------------------------------------------------------------

TEST_CASE(SmartHttp, PackUnpackStream)
{
    const fs::path src_dir = make_temp_dir("pack_stream_src");
    const fs::path dst_dir = make_temp_dir("pack_stream_dst");

    ObjectDatabase src_db(src_dir);
    Blob b1("Hello World Smart HTTP\n");
    std::string sha_b1 = b1.id();
    src_db.write(sha_b1, b1.serialized());

    Blob b2("Second blob data\n");
    std::string sha_b2 = b2.id();
    src_db.write(sha_b2, b2.serialized());

    Tree tree({
        {"100644", "hello.txt", sha_b1},
        {"100644", "test.txt", sha_b2}
    });
    std::string sha_tree = tree.id();
    src_db.write(sha_tree, tree.serialized());

    Commit commit(sha_tree, {}, "Tester <test@example.com>", "Initial pack test commit\n");
    std::string sha_commit = commit.id();
    src_db.write(sha_commit, commit.serialized());

    // Build pack entries
    std::vector<minigit::storage::PackEntry> entries;
    auto add_pe = [&](const std::string &id, const std::string &env, const std::string &type, int num) {
        minigit::storage::PackEntry pe;
        pe.id = id;
        pe.type_name = type;
        pe.type_num = num;
        pe.payload = env.substr(env.find('\0') + 1);
        entries.push_back(std::move(pe));
    };

    add_pe(sha_b1, b1.serialized(), "blob", minigit::storage::OBJ_BLOB);
    add_pe(sha_b2, b2.serialized(), "blob", minigit::storage::OBJ_BLOB);
    add_pe(sha_tree, tree.serialized(), "tree", minigit::storage::OBJ_TREE);
    add_pe(sha_commit, commit.serialized(), "commit", minigit::storage::OBJ_COMMIT);

    fs::path temp_pack_dir = src_dir / "pack_temp";
    fs::create_directories(temp_pack_dir);
    minigit::storage::PackWriteResult pwrite = minigit::storage::write_pack(temp_pack_dir, entries);

    std::ifstream pf(pwrite.pack_path, std::ios::binary);
    std::string pack_bytes((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
    pf.close();

    // 1. Direct raw extraction test
    std::string extracted_raw = extract_pack_stream(pack_bytes);
    ASSERT_EQ(extracted_raw, pack_bytes);

    // 2. Multiplexed side-band stream test
    std::string sideband_stream;
    size_t chunk_sz = 1024;
    for (size_t i = 0; i < pack_bytes.size(); i += chunk_sz)
    {
        std::string chunk = "\x01" + pack_bytes.substr(i, chunk_sz);
        sideband_stream += pkt_line(chunk);
    }
    sideband_stream += pkt_flush();

    std::string extracted_sideband = extract_pack_stream(sideband_stream);
    ASSERT_EQ(extracted_sideband, pack_bytes);

    // 3. Unpack into dst_dir
    size_t unpacked_count = unpack_pack_stream_to_db(pack_bytes, dst_dir);
    ASSERT_EQ(unpacked_count, 4);

    ObjectDatabase dst_db(dst_dir);
    ASSERT_TRUE(dst_db.contains(sha_b1));
    ASSERT_TRUE(dst_db.contains(sha_b2));
    ASSERT_TRUE(dst_db.contains(sha_tree));
    ASSERT_TRUE(dst_db.contains(sha_commit));

    ASSERT_EQ(dst_db.read(sha_b1), b1.serialized());
    ASSERT_EQ(dst_db.read(sha_commit), commit.serialized());

    remove_temp_dir(src_dir);
    remove_temp_dir(dst_dir);
}

// ---------------------------------------------------------------------------
// End-to-End Tests: Clone, Fetch, Push over Smart HTTP
// ---------------------------------------------------------------------------

TEST_CASE(SmartHttp, EndToEndCloneFetchPush)
{
    const fs::path remote_root = make_temp_dir("e2e_remote_repo");
    const fs::path clone_root  = make_temp_dir("e2e_cloned_repo");

    // Initialize remote repository
    Repository remote_repo(remote_root);
    remote_repo.init();

    ObjectDatabase remote_db(remote_repo.objects_dir());
    Blob b1("Remote Initial Content\n");
    std::string sha_b1 = b1.id();
    remote_db.write(sha_b1, b1.serialized());

    Tree tree1({{"100644", "readme.txt", sha_b1}});
    std::string sha_tree1 = tree1.id();
    remote_db.write(sha_tree1, tree1.serialized());

    Commit c1(sha_tree1, {}, "Remote Author <remote@example.com>", "Initial remote commit\n");
    std::string sha_c1 = c1.id();
    remote_db.write(sha_c1, c1.serialized());

    write_file_text(remote_repo.git_dir() / "refs" / "heads" / "main", sha_c1 + "\n");
    write_file_text(remote_repo.git_dir() / "HEAD", "ref: refs/heads/main\n");

    // Start mock HTTP server
    MockGitHttpServer server(remote_root);
    std::string server_url = server.base_url();

    // 1. CLONE
    clone_http(server_url, clone_root.string());

    Repository cloned_repo = Repository::discover(clone_root);
    ASSERT_TRUE(fs::exists(cloned_repo.git_dir()));
    ASSERT_TRUE(fs::exists(clone_root / "readme.txt"));
    ASSERT_EQ(read_file_text(clone_root / "readme.txt"), "Remote Initial Content");

    RemoteConfig cfg(cloned_repo.git_dir() / "config");
    const RemoteEntry *orig = cfg.find("origin");
    ASSERT_TRUE(orig != nullptr);
    ASSERT_EQ(orig->url, server_url);

    std::string local_main_sha = read_file_text(cloned_repo.git_dir() / "refs" / "heads" / "main");
    ASSERT_EQ(local_main_sha, sha_c1);

    std::string tracking_sha = read_file_text(cloned_repo.git_dir() / "refs" / "remotes" / "origin" / "main");
    ASSERT_EQ(tracking_sha, sha_c1);

    // 2. FETCH (Add a second commit to remote repo first)
    Blob b2("Updated Remote Content\n");
    std::string sha_b2 = b2.id();
    remote_db.write(sha_b2, b2.serialized());

    Tree tree2({{"100644", "readme.txt", sha_b2}});
    std::string sha_tree2 = tree2.id();
    remote_db.write(sha_tree2, tree2.serialized());

    Commit c2(sha_tree2, {sha_c1}, "Remote Author <remote@example.com>", "Second remote commit\n");
    std::string sha_c2 = c2.id();
    remote_db.write(sha_c2, c2.serialized());

    write_file_text(remote_repo.git_dir() / "refs" / "heads" / "main", sha_c2 + "\n");

    fetch_http(cloned_repo, "origin", server_url);

    std::string new_tracking_sha = read_file_text(cloned_repo.git_dir() / "refs" / "remotes" / "origin" / "main");
    ASSERT_EQ(new_tracking_sha, sha_c2);

    ObjectDatabase cloned_db(cloned_repo.objects_dir());
    ASSERT_TRUE(cloned_db.contains(sha_c2));
    ASSERT_TRUE(cloned_db.contains(sha_b2));

    // 3. PUSH (Make a third commit in cloned repo on top of c2, then push)
    write_file_text(cloned_repo.git_dir() / "refs" / "heads" / "main", sha_c2 + "\n");

    Blob b3("Local Contributed Content\n");
    std::string sha_b3 = b3.id();
    cloned_db.write(sha_b3, b3.serialized());

    Tree tree3({
        {"100644", "local.txt", sha_b3},
        {"100644", "readme.txt", sha_b2}
    });
    std::string sha_tree3 = tree3.id();
    cloned_db.write(sha_tree3, tree3.serialized());

    Commit c3(sha_tree3, {sha_c2}, "Local Contributor <local@example.com>", "Local commit 3\n");
    std::string sha_c3 = c3.id();
    cloned_db.write(sha_c3, c3.serialized());

    write_file_text(cloned_repo.git_dir() / "refs" / "heads" / "main", sha_c3 + "\n");

    push_http(cloned_repo, "origin", server_url, "main");

    // Verify remote received commit 3 and updated refs/heads/main
    std::string final_remote_sha = read_file_text(remote_repo.git_dir() / "refs" / "heads" / "main");
    ASSERT_EQ(final_remote_sha, sha_c3);
    ASSERT_TRUE(remote_db.contains(sha_c3));
    ASSERT_TRUE(remote_db.contains(sha_b3));

    // Verify local tracking ref was also updated
    std::string final_tracking_sha = read_file_text(cloned_repo.git_dir() / "refs" / "remotes" / "origin" / "main");
    ASSERT_EQ(final_tracking_sha, sha_c3);

    remove_temp_dir(remote_root);
    remove_temp_dir(clone_root);
}
