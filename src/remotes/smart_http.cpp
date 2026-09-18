#include "smart_http.h"

#include "config.h"
#include "http_client.h"
#include "pack_unpack.h"
#include "pkt_line.h"
#include "transfer.h"
#include "repository/repository.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"
#include "storage/pack.h"
#include "staging/index.h"
#include "core/path_safety.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace minigit::remotes {

namespace {

void write_text_file(const fs::path &path, const std::string &content)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("Could not write file: " + path.string());
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string read_text_file(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string s = ss.str();
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

std::string extract_repo_name_from_url(const std::string &url)
{
    std::string clean = normalize_url(url);
    size_t last_slash = clean.rfind('/');
    std::string name = (last_slash != std::string::npos) ? clean.substr(last_slash + 1) : clean;
    if (name.size() > 4 && name.rfind(".git") == name.size() - 4)
        name = name.substr(0, name.size() - 4);
    return name.empty() ? "repo" : name;
}

void populate_working_tree(
    const fs::path &root,
    const ObjectDatabase &db,
    const std::string &tree_sha,
    Index &idx,
    const std::string &prefix = "")
{
    ParsedTree tree = parse_tree(db.read(tree_sha));
    for (const auto &entry : tree.entries)
    {
        std::string rel_path = prefix.empty() ? entry.name : (prefix + "/" + entry.name);
        if (entry.mode == "160000")
        {
            // Submodule gitlink
            idx.add(rel_path, entry.id);
            continue;
        }
        if (entry.mode == "040000" || entry.mode == "40000")
        {
            // Subtree
            populate_working_tree(root, db, entry.id, idx, rel_path);
            continue;
        }

        std::string content = strip_object_header(db.read(entry.id));
        fs::path abs_path = minigit::core::resolve_safe_repo_path(root, rel_path);
        fs::create_directories(abs_path.parent_path());
        std::ofstream out(abs_path, std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("Could not write file: " + abs_path.string());
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        idx.add(rel_path, entry.id);
    }
}

void collect_objects_for_push(
    const ObjectDatabase &db,
    const std::string &new_commit_sha,
    const std::string &old_commit_sha,
    std::vector<minigit::storage::PackEntry> &entries)
{
    std::unordered_set<std::string> stop_commits;
    if (!old_commit_sha.empty() && old_commit_sha != std::string(64, '0'))
    {
        std::queue<std::string> sq;
        sq.push(old_commit_sha);
        while (!sq.empty())
        {
            std::string c = sq.front();
            sq.pop();
            if (c.empty() || stop_commits.count(c)) continue;
            stop_commits.insert(c);
            try
            {
                ParsedCommit pc = parse_commit(db.read(c));
                for (const auto &p : pc.parent_ids)
                    if (!p.empty()) sq.push(p);
            }
            catch (...) {}
        }
    }

    std::unordered_set<std::string> visited_shas;
    std::queue<std::string> commit_queue;
    commit_queue.push(new_commit_sha);

    while (!commit_queue.empty())
    {
        std::string cur = commit_queue.front();
        commit_queue.pop();
        if (cur.empty() || stop_commits.count(cur) || visited_shas.count(cur))
            continue;
        visited_shas.insert(cur);

        std::string commit_envelope = db.read(cur);
        ParsedCommit pc = parse_commit(commit_envelope);

        // Add commit object
        size_t null_pos = commit_envelope.find('\0');
        minigit::storage::PackEntry ce;
        ce.id = cur;
        ce.type_name = "commit";
        ce.type_num = minigit::storage::OBJ_COMMIT;
        ce.payload = commit_envelope.substr(null_pos + 1);
        entries.push_back(std::move(ce));

        // Queue parents
        for (const auto &p : pc.parent_ids)
        {
            if (!p.empty() && !stop_commits.count(p))
                commit_queue.push(p);
        }

        // Collect trees and blobs recursively
        std::queue<std::string> tree_queue;
        tree_queue.push(pc.tree_id);

        while (!tree_queue.empty())
        {
            std::string t = tree_queue.front();
            tree_queue.pop();
            if (t.empty() || visited_shas.count(t)) continue;
            visited_shas.insert(t);

            std::string tree_envelope = db.read(t);
            ParsedTree pt = parse_tree(tree_envelope);

            size_t t_null = tree_envelope.find('\0');
            minigit::storage::PackEntry te;
            te.id = t;
            te.type_name = "tree";
            te.type_num = minigit::storage::OBJ_TREE;
            te.payload = tree_envelope.substr(t_null + 1);
            entries.push_back(std::move(te));

            for (const auto &e : pt.entries)
            {
                if (e.mode == "040000" || e.mode == "40000")
                {
                    tree_queue.push(e.id);
                }
                else if (e.mode != "160000" && !visited_shas.count(e.id))
                {
                    visited_shas.insert(e.id);
                    std::string blob_envelope = db.read(e.id);
                    size_t b_null = blob_envelope.find('\0');
                    minigit::storage::PackEntry be;
                    be.id = e.id;
                    be.type_name = "blob";
                    be.type_num = minigit::storage::OBJ_BLOB;
                    be.payload = blob_envelope.substr(b_null + 1);
                    entries.push_back(std::move(be));
                }
            }
        }
    }
}

} // namespace

bool is_http_url(const std::string &url)
{
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}

std::string normalize_url(const std::string &url)
{
    std::string s = url;
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    return s;
}

AdvertisedRefsResult discover_upload_pack(const std::string &url)
{
    std::string target = normalize_url(url) + "/info/refs?service=git-upload-pack";
    HttpClient client;
    HttpResponse resp = client.get(target, {"Accept: */*"});

    if (!resp.ok())
    {
        AdvertisedRefsResult err;
        err.error = resp.error.empty() ? ("HTTP " + std::to_string(resp.status_code)) : resp.error;
        return err;
    }

    return parse_advertised_refs(resp.body, "git-upload-pack");
}

AdvertisedRefsResult discover_receive_pack(const std::string &url)
{
    std::string target = normalize_url(url) + "/info/refs?service=git-receive-pack";
    HttpClient client;
    HttpResponse resp = client.get(target, {"Accept: */*"});

    if (!resp.ok())
    {
        AdvertisedRefsResult err;
        err.error = resp.error.empty() ? ("HTTP " + std::to_string(resp.status_code)) : resp.error;
        return err;
    }

    return parse_advertised_refs(resp.body, "git-receive-pack");
}

void clone_http(const std::string &url, const std::string &dest_dir_str)
{
    std::string norm_url = normalize_url(url);

    // 1. Discover refs
    AdvertisedRefsResult discovery = discover_upload_pack(norm_url);
    if (!discovery.success)
    {
        std::cerr << "fatal: unable to access '" << norm_url << "': " << discovery.error << "\n";
        std::exit(1);
    }

    // Determine target directory
    fs::path dest;
    if (dest_dir_str.empty())
    {
        dest = fs::current_path() / extract_repo_name_from_url(norm_url);
    }
    else
    {
        dest = fs::weakly_canonical(fs::absolute(dest_dir_str));
    }

    if (fs::exists(dest) && !fs::is_empty(dest))
    {
        std::cerr << "fatal: destination '" << dest.string() << "' already exists and is not empty\n";
        std::exit(1);
    }

    std::cout << "Cloning into '" << dest.filename().string() << "'...\n";

    fs::create_directories(dest);
    Repository new_repo(dest);
    new_repo.init();

    if (discovery.refs.empty())
    {
        std::cout << "warning: You appear to have cloned an empty repository.\n";
        RemoteConfig cfg(new_repo.git_dir() / "config");
        cfg.add("origin", norm_url);
        return;
    }

    // Determine primary branch
    std::string branch = "main";
    if (!discovery.symref_head.empty() && discovery.symref_head.rfind("refs/heads/", 0) == 0)
    {
        branch = discovery.symref_head.substr(11);
    }
    else if (discovery.ref_map.count("HEAD"))
    {
        std::string head_sha = discovery.ref_map["HEAD"];
        for (const auto &ref : discovery.refs)
        {
            if (ref.sha == head_sha && ref.name.rfind("refs/heads/", 0) == 0)
            {
                branch = ref.name.substr(11);
                break;
            }
        }
    }

    std::string head_sha = discovery.ref_map.count("refs/heads/" + branch)
                               ? discovery.ref_map["refs/heads/" + branch]
                               : discovery.refs[0].sha;

    // 2. Build upload-pack request (wants)
    std::string request_body;
    bool first_want = true;
    std::unordered_set<std::string> requested_shas;

    for (const auto &ref : discovery.refs)
    {
        if (requested_shas.count(ref.sha)) continue;
        requested_shas.insert(ref.sha);

        if (first_want)
        {
            request_body += pkt_line("want " + ref.sha + " ofs-delta agent=minigit/1.8.0\n");
            first_want = false;
        }
        else
        {
            request_body += pkt_line("want " + ref.sha + "\n");
        }
    }
    request_body += pkt_flush();
    request_body += pkt_line("done\n");

    // 3. Post to /git-upload-pack
    HttpClient client;
    HttpResponse upload_resp = client.post(
        norm_url + "/git-upload-pack",
        request_body,
        "application/x-git-upload-pack-request",
        {"Accept: application/x-git-upload-pack-result"}
    );

    if (!upload_resp.ok())
    {
        std::cerr << "fatal: git-upload-pack failed: " << upload_resp.error
                  << " (HTTP " << upload_resp.status_code << ")\n";
        std::exit(1);
    }

    // 4. Extract and unpack packfile
    std::string pack_stream = extract_pack_stream(upload_resp.body);
    size_t count = unpack_pack_stream_to_db(pack_stream, new_repo.objects_dir());
    std::cout << "Transferred " << count << " object(s).\n";

    // 5. Write references and configure remote
    for (const auto &ref : discovery.refs)
    {
        if (ref.name.rfind("refs/heads/", 0) == 0)
        {
            write_text_file(new_repo.git_dir() / ref.name, ref.sha + "\n");
            std::string bname = ref.name.substr(11);
            write_text_file(new_repo.git_dir() / "refs" / "remotes" / "origin" / bname, ref.sha + "\n");
        }
        else if (ref.name.rfind("refs/tags/", 0) == 0)
        {
            write_text_file(new_repo.git_dir() / ref.name, ref.sha + "\n");
        }
    }

    write_text_file(new_repo.head_path(), "ref: refs/heads/" + branch + "\n");

    RemoteConfig cfg(new_repo.git_dir() / "config");
    cfg.add("origin", norm_url);

    // 6. Checkout HEAD working tree
    ObjectDatabase db(new_repo.objects_dir());
    ParsedCommit commit = parse_commit(db.read(head_sha));
    Index idx(new_repo.index_path());
    populate_working_tree(new_repo.root(), db, commit.tree_id, idx);
    idx.write();
}

void fetch_http(const Repository &local, const std::string &remote_name, const std::string &url)
{
    std::string norm_url = normalize_url(url);

    // 1. Discover advertised refs
    AdvertisedRefsResult discovery = discover_upload_pack(norm_url);
    if (!discovery.success)
    {
        std::cerr << "fatal: unable to access '" << norm_url << "': " << discovery.error << "\n";
        std::exit(1);
    }

    const fs::path remotes_dir = local.git_dir() / "refs" / "remotes" / remote_name;
    fs::create_directories(remotes_dir);

    // 2. Identify new or updated refs
    std::vector<std::string> wants;
    std::unordered_set<std::string> want_set;
    std::vector<std::pair<std::string, std::string>> updated_branches;

    for (const auto &ref : discovery.refs)
    {
        if (ref.name.rfind("refs/heads/", 0) == 0)
        {
            std::string bname = ref.name.substr(11);
            fs::path track_path = remotes_dir / bname;
            std::string current_sha = read_text_file(track_path);

            if (current_sha != ref.sha)
            {
                if (!want_set.count(ref.sha))
                {
                    wants.push_back(ref.sha);
                    want_set.insert(ref.sha);
                }
                updated_branches.push_back({bname, ref.sha});
            }
        }
    }

    if (wants.empty())
    {
        // Everything up to date
        return;
    }

    // 3. Collect local haves
    std::vector<std::string> haves;
    fs::path local_heads = local.git_dir() / "refs" / "heads";
    if (fs::exists(local_heads))
    {
        for (const auto &entry : fs::directory_iterator(local_heads))
        {
            if (entry.is_regular_file())
            {
                std::string s = read_text_file(entry.path());
                if (!s.empty()) haves.push_back(s);
            }
        }
    }

    // 4. Build upload-pack request
    std::string req;
    bool first = true;
    for (const auto &w : wants)
    {
        if (first)
        {
            req += pkt_line("want " + w + " ofs-delta agent=minigit/1.8.0\n");
            first = false;
        }
        else
        {
            req += pkt_line("want " + w + "\n");
        }
    }
    req += pkt_flush();
    for (const auto &h : haves)
    {
        req += pkt_line("have " + h + "\n");
    }
    req += pkt_line("done\n");

    // 5. Send POST to /git-upload-pack
    HttpClient client;
    HttpResponse upload_resp = client.post(
        norm_url + "/git-upload-pack",
        req,
        "application/x-git-upload-pack-request",
        {"Accept: application/x-git-upload-pack-result"}
    );

    if (!upload_resp.ok())
    {
        std::cerr << "fatal: git-upload-pack failed: " << upload_resp.error << "\n";
        std::exit(1);
    }

    // 6. Unpack pack stream
    std::string pack_stream = extract_pack_stream(upload_resp.body);
    unpack_pack_stream_to_db(pack_stream, local.objects_dir());

    // 7. Update remote-tracking refs
    for (const auto &[bname, new_sha] : updated_branches)
    {
        fs::path track_path = remotes_dir / bname;
        std::string old_sha = read_text_file(track_path);
        write_text_file(track_path, new_sha + "\n");

        if (old_sha.empty())
        {
            std::cout << " * [new branch]      " << bname << " -> " << remote_name << "/" << bname << "\n";
        }
        else
        {
            std::cout << "   " << old_sha.substr(0, 7) << ".." << new_sha.substr(0, 7)
                      << "  " << bname << " -> " << remote_name << "/" << bname << "\n";
        }
    }
}

void push_http(
    const Repository &local,
    const std::string &remote_name,
    const std::string &url,
    const std::string &branch_name)
{
    std::string norm_url = normalize_url(url);

    // 1. Discover receive-pack refs
    AdvertisedRefsResult discovery = discover_receive_pack(norm_url);
    if (!discovery.success)
    {
        std::cerr << "fatal: unable to access '" << norm_url << "': " << discovery.error << "\n";
        std::exit(1);
    }

    std::string local_branch_sha = read_text_file(local.git_dir() / "refs" / "heads" / branch_name);
    if (local_branch_sha.empty())
    {
        std::cerr << "error: src refspec " << branch_name << " does not match any\n";
        std::exit(1);
    }

    std::string ref_target = "refs/heads/" + branch_name;
    std::string old_sha = discovery.ref_map.count(ref_target)
                              ? discovery.ref_map[ref_target]
                              : std::string(64, '0');

    if (old_sha == local_branch_sha)
    {
        std::cout << "Everything up-to-date\n";
        return;
    }

    // 2. Fast-forward validation
    if (old_sha != std::string(64, '0'))
    {
        if (!transfer::is_ancestor(local.objects_dir(), old_sha, local_branch_sha))
        {
            std::cerr << "error: failed to push some refs to '" << norm_url << "'\n";
            std::cerr << "hint: Updates were rejected because the remote contains work that you do\n";
            std::cerr << "hint: not have locally. This is usually caused by another repository pushing\n";
            std::cerr << "hint: to the same ref. You may want to first integrate the remote changes\n";
            std::cerr << "hint: (e.g., 'minigit pull ...') before pushing again.\n";
            std::exit(1);
        }
    }

    // 3. Collect objects to push
    ObjectDatabase db(local.objects_dir());
    std::vector<minigit::storage::PackEntry> entries;
    collect_objects_for_push(db, local_branch_sha, old_sha, entries);

    // 4. Create packfile
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path temp_pack_dir = local.objects_dir() / ("temp_push_" + std::to_string(now));
    fs::create_directories(temp_pack_dir);

    minigit::storage::PackWriteResult pwrite = minigit::storage::write_pack(temp_pack_dir, entries);

    std::ifstream pfile(pwrite.pack_path, std::ios::binary);
    if (!pfile)
        throw std::runtime_error("Could not read generated push packfile");
    std::string pack_bytes((std::istreambuf_iterator<char>(pfile)), std::istreambuf_iterator<char>());
    pfile.close();

    // Clean up temporary pack dir
    fs::remove_all(temp_pack_dir);

    // 5. Build receive-pack POST payload
    std::string req;
    req += pkt_line(old_sha + " " + local_branch_sha + " " + ref_target + '\0' + "report-status agent=minigit/1.8.0\n");
    req += pkt_flush();
    req.append(pack_bytes);

    // 6. Send POST to /git-receive-pack
    HttpClient client;
    HttpResponse push_resp = client.post(
        norm_url + "/git-receive-pack",
        req,
        "application/x-git-receive-pack-request",
        {"Accept: application/x-git-receive-pack-result"}
    );

    if (!push_resp.ok())
    {
        std::cerr << "fatal: git-receive-pack failed: " << push_resp.error
                  << " (HTTP " << push_resp.status_code << ")\n";
        std::exit(1);
    }

    // 7. Parse report-status
    if (push_resp.body.find("unpack ok") == std::string::npos &&
        push_resp.body.find("ok " + ref_target) == std::string::npos &&
        !push_resp.body.empty())
    {
        std::cerr << "remote: " << push_resp.body << "\n";
    }

    // 8. Update local remote-tracking ref
    fs::path track_path = local.git_dir() / "refs" / "remotes" / remote_name / branch_name;
    write_text_file(track_path, local_branch_sha + "\n");

    std::cout << "To " << norm_url << "\n";
    if (old_sha == std::string(64, '0'))
    {
        std::cout << " * [new branch]      " << branch_name << " -> " << branch_name << "\n";
    }
    else
    {
        std::cout << "   " << old_sha.substr(0, 7) << ".." << local_branch_sha.substr(0, 7)
                  << "  " << branch_name << " -> " << branch_name << "\n";
    }
}

} // namespace minigit::remotes
