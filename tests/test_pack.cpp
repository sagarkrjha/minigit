#include "test_framework.h"
#include "storage/pack.h"
#include "storage/repack.h"
#include "storage/object_database.h"
#include "storage/blob.h"
#include "storage/commit.h"
#include "storage/tree.h"
#include "storage/object_parser.h"
#include "core/sha256.h"
#include "repository/repository.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace minigit::storage;

namespace {

fs::path make_temp_dir(const std::string& prefix)
{
    const auto p = fs::temp_directory_path() / ("minigit_test_" + prefix + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    fs::create_directories(p);
    return p;
}

void remove_temp_dir(const fs::path& p)
{
    std::error_code ec;
    fs::remove_all(p, ec);
}

} // namespace

TEST_CASE(Packfile, DeltaCompressionRoundtrip)
{
    const std::string base = "The quick brown fox jumps over the lazy dog. "
                             "Packfiles reduce disk footprint using sliding window deltas.";
    const std::string target = "The quick brown fox leaps over the very lazy dog. "
                               "Packfiles dramatically reduce disk footprint using sliding window deltas! Extra content added.";

    const std::string delta = create_delta(base, target);
    ASSERT_FALSE(delta.empty());

    const std::string reconstituted = apply_delta(base, delta);
    ASSERT_EQ(reconstituted, target);
}

TEST_CASE(Packfile, DeltaCompressionLargeBinary)
{
    std::string base(2048, 'A');
    for (size_t i = 0; i < base.size(); i += 17)
    {
        base[i] = static_cast<char>('0' + (i % 10));
    }

    std::string target = base;
    // Modify middle chunk
    target.replace(500, 200, std::string(200, 'Z'));
    // Append extra chunk
    target.append(std::string(300, 'X'));

    const std::string delta = create_delta(base, target);
    ASSERT_TRUE(delta.size() < target.size());

    const std::string reconstituted = apply_delta(base, delta);
    ASSERT_EQ(reconstituted, target);
}

TEST_CASE(Packfile, WriteAndReadPack)
{
    const fs::path temp_dir = make_temp_dir("write_read_pack");

    // Construct 3 distinct objects
    std::vector<PackEntry> entries;

    // 1. Blob
    const std::string blob_content = "Hello, world! This is a test blob stored in a packfile.\n";
    const std::string blob_envelope = "blob " + std::to_string(blob_content.size()) + '\0' + blob_content;
    const std::string blob_sha = sha256(blob_envelope);
    entries.push_back({blob_sha, "blob", OBJ_BLOB, blob_content, false, "", ""});

    // 2. Tree
    const std::string tree_content = "100644 hello.txt\0" + blob_sha;
    const std::string tree_envelope = "tree " + std::to_string(tree_content.size()) + '\0' + tree_content;
    const std::string tree_sha = sha256(tree_envelope);
    entries.push_back({tree_sha, "tree", OBJ_TREE, tree_content, false, "", ""});

    // 3. Commit
    const std::string commit_content = "tree " + tree_sha + "\nauthor Tester <test@example.com> 1700000000 +0000\n\nInitial commit\n";
    const std::string commit_envelope = "commit " + std::to_string(commit_content.size()) + '\0' + commit_content;
    const std::string commit_sha = sha256(commit_envelope);
    entries.push_back({commit_sha, "commit", OBJ_COMMIT, commit_content, false, "", ""});

    const PackWriteResult res = write_pack(temp_dir, entries);
    ASSERT_EQ(res.total_objects, 3);
    ASSERT_EQ(res.delta_objects, 0);
    ASSERT_TRUE(fs::exists(res.pack_path));
    ASSERT_TRUE(fs::exists(res.idx_path));

    // Open index and check lookups
    auto idx = PackIndex::open(res.idx_path);
    ASSERT_TRUE(idx != nullptr);
    ASSERT_EQ(idx->count(), 3);

    const int64_t blob_off = idx->find_offset(blob_sha);
    const int64_t tree_off = idx->find_offset(tree_sha);
    const int64_t commit_off = idx->find_offset(commit_sha);
    ASSERT_TRUE(blob_off >= 12);
    ASSERT_TRUE(tree_off >= 12);
    ASSERT_TRUE(commit_off >= 12);
    ASSERT_EQ(idx->find_offset("0000000000000000000000000000000000000000000000000000000000000000"), -1);

    // Open reader and read objects
    ObjectDatabase empty_db(temp_dir);
    auto reader = PackReader::open(res.pack_path);
    ASSERT_TRUE(reader != nullptr);

    const std::string read_blob = reader->read_object(static_cast<uint64_t>(blob_off), empty_db);
    ASSERT_EQ(read_blob, blob_envelope);

    const std::string read_tree = reader->read_object(static_cast<uint64_t>(tree_off), empty_db);
    ASSERT_EQ(read_tree, tree_envelope);

    const std::string read_commit = reader->read_object(static_cast<uint64_t>(commit_off), empty_db);
    ASSERT_EQ(read_commit, commit_envelope);

    remove_temp_dir(temp_dir);
}

TEST_CASE(Packfile, RefDeltaDecompression)
{
    const fs::path temp_dir = make_temp_dir("ref_delta");

    const std::string base_text = "Line 1: Configuration standard\nLine 2: Server port = 8080\nLine 3: Max connections = 100\n";
    const std::string target_text = "Line 1: Configuration standard\nLine 2: Server port = 9090\nLine 3: Max connections = 500\nLine 4: SSL = true\n";

    const std::string base_env = "blob " + std::to_string(base_text.size()) + '\0' + base_text;
    const std::string target_env = "blob " + std::to_string(target_text.size()) + '\0' + target_text;

    const std::string base_sha = sha256(base_env);
    const std::string target_sha = sha256(target_env);

    const std::string delta_bytes = create_delta(base_text, target_text);

    std::vector<PackEntry> entries;
    entries.push_back({base_sha, "blob", OBJ_BLOB, base_text, false, "", ""});
    entries.push_back({target_sha, "blob", OBJ_BLOB, target_text, true, base_sha, delta_bytes});

    const PackWriteResult res = write_pack(temp_dir, entries);
    ASSERT_EQ(res.total_objects, 2);
    ASSERT_EQ(res.delta_objects, 1);

    // Setup an ObjectDatabase that contains the base object (so OBJ_REF_DELTA can resolve it)
    ObjectDatabase db(temp_dir / "objects");
    db.write(base_sha, base_env);

    auto idx = PackIndex::open(res.idx_path);
    ASSERT_TRUE(idx != nullptr);
    const int64_t target_off = idx->find_offset(target_sha);
    ASSERT_TRUE(target_off >= 12);

    auto reader = PackReader::open(res.pack_path);
    ASSERT_TRUE(reader != nullptr);

    const std::string resolved = reader->read_object(static_cast<uint64_t>(target_off), db);
    ASSERT_EQ(resolved, target_env);
    ASSERT_EQ(sha256(resolved), target_sha);

    remove_temp_dir(temp_dir);
}

TEST_CASE(Packfile, RepackRepositoryAndTransparentRead)
{
    const fs::path repo_dir = make_temp_dir("repack_repo");
    Repository repo(repo_dir);
    repo.init();

    ObjectDatabase db(repo.git_dir() / "objects");

    // Write multiple objects to loose storage
    std::vector<std::string> written_ids;
    std::vector<std::string> written_payloads;

    for (int i = 0; i < 15; ++i)
    {
        const std::string content = "Version " + std::to_string(i) + " of repeated codebase documentation string for testing packfiles.\n";
        const std::string env = "blob " + std::to_string(content.size()) + '\0' + content;
        const std::string id = sha256(env);

        db.write(id, env);
        written_ids.push_back(id);
        written_payloads.push_back(env);
    }

    // Repack with delete_loose = true
    RepackOptions opts;
    opts.pack_all = true;
    opts.delete_loose = true;
    opts.window = 5;

    const RepackResult repack_res = repack_repository(repo_dir, opts);
    ASSERT_TRUE(repack_res.pack_created);
    ASSERT_EQ(repack_res.loose_objects_found, 15);
    ASSERT_EQ(repack_res.loose_objects_deleted, 15);
    ASSERT_TRUE(repack_res.pack_result.delta_objects > 0);

    // Verify all loose object files are indeed gone
    for (const auto& id : written_ids)
    {
        const auto loose_file = repo.git_dir() / "objects" / id.substr(0, 2) / id.substr(2);
        ASSERT_FALSE(fs::exists(loose_file));
    }

    // Now reload database packs and verify every single object is read transparently!
    db.reload_packs();
    for (size_t i = 0; i < written_ids.size(); ++i)
    {
        ASSERT_TRUE(db.contains(written_ids[i]));
        const std::string read_env = db.read(written_ids[i]);
        ASSERT_EQ(read_env, written_payloads[i]);
    }

    // Verify verify_pack_file works
    ASSERT_TRUE(verify_pack_file(repack_res.pack_result.pack_path, false));
    ASSERT_TRUE(verify_pack_file(repack_res.pack_result.idx_path, false));

    remove_temp_dir(repo_dir);
}

TEST_CASE(Packfile, VerifyPackDetectsCorruption)
{
    const fs::path repo_dir = make_temp_dir("verify_corrupt");
    Repository repo(repo_dir);
    repo.init();

    ObjectDatabase db(repo.git_dir() / "objects");
    const std::string c1 = "Payload 1 for verify test.\n";
    const std::string env1 = "blob " + std::to_string(c1.size()) + '\0' + c1;
    const std::string id1 = sha256(env1);
    db.write(id1, env1);

    const RepackResult repack_res = repack_repository(repo_dir, RepackOptions{.delete_loose = true});
    ASSERT_TRUE(repack_res.pack_created);

    // Valid file returns true
    ASSERT_TRUE(verify_pack_file(repack_res.pack_result.pack_path, false));

    // Corrupt one byte in the packfile
    {
        std::fstream file(repack_res.pack_result.pack_path, std::ios::in | std::ios::out | std::ios::binary);
        file.seekp(14, std::ios::beg); // byte inside first object
        char byte = 0;
        file.read(&byte, 1);
        byte ^= 0xFF; // flip bits
        file.seekp(14, std::ios::beg);
        file.write(&byte, 1);
    }

    // Should now detect checksum/CRC error and return false
    ASSERT_FALSE(verify_pack_file(repack_res.pack_result.pack_path, false));

    remove_temp_dir(repo_dir);
}

TEST_CASE(Packfile, CommitHistoryTraversalAfterRepack)
{
    const fs::path repo_dir = make_temp_dir("commit_traverse");
    Repository repo(repo_dir);
    repo.init();

    ObjectDatabase db(repo.git_dir() / "objects");

    // 1. Create commit 1 with blob 1
    const std::string b1_data = "int main() { return 0; }\n";
    Blob blob1(b1_data);
    db.write(blob1.id(), blob1.serialized());

    Tree tree1({TreeEntry{"100644", "main.cpp", blob1.id()}});
    db.write(tree1.id(), tree1.serialized());

    Commit c1(tree1.id(), {}, "Author <a@b.c>", "Initial commit");
    db.write(c1.id(), c1.serialized());

    // 2. Create commit 2 with blob 2
    const std::string b2_data = "int main() { return 42; }\n";
    Blob blob2(b2_data);
    db.write(blob2.id(), blob2.serialized());

    Tree tree2({TreeEntry{"100644", "main.cpp", blob2.id()}});
    db.write(tree2.id(), tree2.serialized());

    Commit c2(tree2.id(), {c1.id()}, "Author <a@b.c>", "Second commit");
    db.write(c2.id(), c2.serialized());

    // 3. Repack and delete all loose objects
    RepackOptions opts;
    opts.delete_loose = true;
    RepackResult r = repack_repository(repo_dir, opts);
    ASSERT_TRUE(r.pack_created);
    ASSERT_EQ(r.loose_objects_deleted, 6);

    // 4. Reload packs and traverse history purely from packfile!
    db.reload_packs();

    const std::string raw_c2 = db.read(c2.id());
    ParsedCommit pc2 = parse_commit(raw_c2);
    ASSERT_EQ(pc2.tree_id, tree2.id());
    ASSERT_EQ(pc2.parent_ids.size(), 1);
    ASSERT_EQ(pc2.parent_ids[0], c1.id());

    const std::string raw_t2 = db.read(pc2.tree_id);
    ParsedTree pt2 = parse_tree(raw_t2);
    ASSERT_EQ(pt2.entries.size(), 1);
    ASSERT_EQ(pt2.entries[0].name, "main.cpp");
    ASSERT_EQ(pt2.entries[0].id, blob2.id());

    const std::string raw_b2 = db.read(pt2.entries[0].id);
    ASSERT_EQ(strip_object_header(raw_b2), b2_data);

    // Parent commit c1
    const std::string raw_c1 = db.read(pc2.parent_ids[0]);
    ParsedCommit pc1 = parse_commit(raw_c1);
    ASSERT_EQ(pc1.tree_id, tree1.id());
    ASSERT_TRUE(pc1.parent_ids.empty());

    const std::string raw_t1 = db.read(pc1.tree_id);
    ParsedTree pt1 = parse_tree(raw_t1);
    ASSERT_EQ(pt1.entries[0].id, blob1.id());

    const std::string raw_b1 = db.read(pt1.entries[0].id);
    ASSERT_EQ(strip_object_header(raw_b1), b1_data);

    ASSERT_TRUE(verify_pack_file(r.pack_result.pack_path, true));

    remove_temp_dir(repo_dir);
}

