#include "test_framework.h"
#include "merge/cherry_pick.h"
#include "repository/repository.h"
#include "staging/index.h"
#include "storage/blob.h"
#include "storage/commit.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"
#include "storage/tree.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

void write_file(const fs::path& p, const std::string& content)
{
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string read_file(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::string s;
    f.seekg(0, std::ios::end);
    s.resize(f.tellg());
    f.seekg(0, std::ios::beg);
    f.read(&s[0], s.size());
    return s;
}

std::string read_single_line(const fs::path& p)
{
    std::ifstream f(p);
    if (!f) return {};
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
        line.pop_back();
    return line;
}

// Helper to stage files and record in index, then commit
void stage_and_commit(const fs::path& repo_root,
                      const std::vector<std::pair<std::string, std::string>>& files_to_write,
                      const std::vector<std::string>& files_to_delete,
                      const std::string& message,
                      const std::string& author = "Test User <test@example.com>")
{
    const fs::path git_dir = repo_root / ".minigit";
    ObjectDatabase db(git_dir / "objects");
    Index index(git_dir / "index");

    // Process writes
    for (const auto& [rel_path, content] : files_to_write)
    {
        write_file(repo_root / rel_path, content);
        Blob b(content);
        db.write(b.id(), b.serialized());
        index.add(rel_path, b.id());
    }

    // Process deletes
    for (const auto& rel_path : files_to_delete)
    {
        std::error_code ec;
        fs::remove(repo_root / rel_path, ec);
        index.remove(rel_path);
    }
    index.write();

    // Create tree
    std::vector<TreeEntry> entries;
    for (const auto& [path, id] : index.entries())
    {
        entries.push_back({"100644", path, id});
    }
    Tree tree(entries);
    db.write(tree.id(), tree.serialized());

    // Find HEAD parent
    std::vector<std::string> parents;
    const std::string head_raw = read_single_line(git_dir / "HEAD");

    fs::path ref_path = git_dir / "HEAD";
    if (head_raw.rfind("ref: ", 0) == 0)
    {
        ref_path = git_dir / head_raw.substr(5);
        if (fs::exists(ref_path))
        {
            const std::string parent_sha = read_single_line(ref_path);
            if (!parent_sha.empty())
                parents.push_back(parent_sha);
        }
    }
    else if (!head_raw.empty())
    {
        parents.push_back(head_raw);
    }

    // Ensure timestamp ordering
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    Commit c(tree.id(), parents, author, message);
    db.write(c.id(), c.serialized());

    fs::create_directories(ref_path.parent_path());
    {
        std::ofstream ref_out(ref_path, std::ios::trunc);
        ref_out << c.id() << '\n';
    }
}

void switch_branch(const fs::path& repo_root, const std::string& branch_name)
{
    const fs::path git_dir = repo_root / ".minigit";
    const fs::path branch_ref = git_dir / "refs" / "heads" / branch_name;
    const fs::path head_file = git_dir / "HEAD";

    {
        std::ofstream h(head_file, std::ios::trunc);
        h << "ref: refs/heads/" << branch_name << '\n';
    }

    if (fs::exists(branch_ref))
    {
        const std::string commit_sha = read_single_line(branch_ref);

        ObjectDatabase db(git_dir / "objects");
        const ParsedCommit c = parse_commit(db.read(commit_sha));
        const ParsedTree t = parse_tree(db.read(c.tree_id));

        // Remove files currently in index from working tree
        Index old_idx(git_dir / "index");
        for (const auto& [name, _] : old_idx.entries())
        {
            std::error_code ec;
            fs::remove(repo_root / name, ec);
        }

        // Clear index and load target branch entries
        {
            std::ofstream clr(git_dir / "index", std::ios::trunc);
        }
        Index fresh(git_dir / "index");
        for (const auto& entry : t.entries)
        {
            const std::string body = strip_object_header(db.read(entry.id));
            write_file(repo_root / entry.name, body);
            fresh.add(entry.name, entry.id);
        }
        fresh.write();
    }
}

void create_branch(const fs::path& repo_root, const std::string& branch_name)
{
    const fs::path git_dir = repo_root / ".minigit";
    const std::string head_raw = read_single_line(git_dir / "HEAD");

    std::string current_sha;
    if (head_raw.rfind("ref: ", 0) == 0)
    {
        current_sha = read_single_line(git_dir / head_raw.substr(5));
    }
    else
    {
        current_sha = head_raw;
    }

    const fs::path branch_file = git_dir / "refs" / "heads" / branch_name;
    fs::create_directories(branch_file.parent_path());
    std::ofstream b_out(branch_file, std::ios::trunc);
    b_out << current_sha << '\n';
}

} // namespace

TEST_CASE(CherryPick, CleanApplyAcrossBranches)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_cp_clean_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    // 1. Initial commit on main
    stage_and_commit(temp_dir, {{"fileA.txt", "Initial A\n"}}, {}, "Initial commit on main");

    // 2. Branch out to feature
    create_branch(temp_dir, "feature");
    switch_branch(temp_dir, "feature");

    // 3. Commit on feature branch
    stage_and_commit(temp_dir, {{"fileB.txt", "Feature B content\n"}}, {}, "Add Feature B", "Feature Dev <dev@feature.com>");

    // Capture the feature commit SHA
    const std::string feat_commit_sha = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "feature");

    // 4. Another commit on feature branch
    stage_and_commit(temp_dir, {{"fileC.txt", "Feature C content\n"}}, {}, "Add Feature C");

    // 5. Switch back to main
    switch_branch(temp_dir, "main");
    ASSERT_FALSE(fs::exists(temp_dir / "fileB.txt"));

    // 6. Cherry-pick feat_commit_sha onto main
    CherryPickResult result = perform_cherry_pick(temp_dir, feat_commit_sha);

    ASSERT_TRUE(result.success);
    ASSERT_FALSE(result.conflict);
    ASSERT_FALSE(result.new_commit_sha.empty());
    ASSERT_TRUE(fs::exists(temp_dir / "fileB.txt"));
    ASSERT_EQ(read_file(temp_dir / "fileB.txt"), "Feature B content\n");

    // Verify commit object metadata
    {
        ObjectDatabase db(temp_dir / ".minigit" / "objects");
        const ParsedCommit new_commit = parse_commit(db.read(result.new_commit_sha));
        ASSERT_EQ(new_commit.message, "Add Feature B");
        ASSERT_EQ(new_commit.author, "Feature Dev <dev@feature.com>");
        ASSERT_EQ(new_commit.parent_ids.size(), 1);
    }

    // Verify index is consistent
    {
        Index idx(temp_dir / ".minigit" / "index");
        ASSERT_TRUE(idx.entries().count("fileA.txt") > 0);
        ASSERT_TRUE(idx.entries().count("fileB.txt") > 0);
        ASSERT_FALSE(idx.entries().count("fileC.txt") > 0);
    }

    fs::remove_all(temp_dir, ec);
}

TEST_CASE(CherryPick, NoCommitOption)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_cp_nocommit_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    stage_and_commit(temp_dir, {{"hello.txt", "Hello World\n"}}, {}, "Initial commit");

    create_branch(temp_dir, "patch");
    switch_branch(temp_dir, "patch");
    stage_and_commit(temp_dir, {{"patch.txt", "Patch data\n"}}, {}, "Add patch file");

    switch_branch(temp_dir, "main");

    const std::string main_sha_before = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "main");

    // Perform cherry-pick with no_commit = true
    CherryPickResult result = perform_cherry_pick(temp_dir, "patch", "", true);

    ASSERT_TRUE(result.success);
    ASSERT_FALSE(result.conflict);
    ASSERT_TRUE(result.new_commit_sha.empty()); // No commit made!

    // Working directory and index should have patch.txt
    ASSERT_TRUE(fs::exists(temp_dir / "patch.txt"));
    ASSERT_EQ(read_file(temp_dir / "patch.txt"), "Patch data\n");

    {
        Index idx(temp_dir / ".minigit" / "index");
        ASSERT_TRUE(idx.entries().count("patch.txt") > 0);
    }

    // HEAD commit on main should NOT have changed
    const std::string main_sha_after = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "main");
    ASSERT_EQ(main_sha_before, main_sha_after);

    fs::remove_all(temp_dir, ec);
}

TEST_CASE(CherryPick, ContentConflictDetection)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_cp_conflict_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    stage_and_commit(temp_dir, {{"shared.txt", "Line 1: Base\nLine 2: Common\n"}}, {}, "Base commit");

    create_branch(temp_dir, "branchA");
    switch_branch(temp_dir, "branchA");
    stage_and_commit(temp_dir, {{"shared.txt", "Line 1: BranchA modification\nLine 2: Common\n"}}, {}, "BranchA modification");

    switch_branch(temp_dir, "main");
    stage_and_commit(temp_dir, {{"shared.txt", "Line 1: Main branch modification\nLine 2: Common\n"}}, {}, "Main modification");

    // Cherry-pick branchA onto main -> should produce conflict on shared.txt
    CherryPickResult result = perform_cherry_pick(temp_dir, "branchA");

    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.conflict);
    ASSERT_EQ(result.conflicted_files.size(), 1);
    ASSERT_EQ(result.conflicted_files[0], "shared.txt");

    // File should contain conflict markers
    const std::string content = read_file(temp_dir / "shared.txt");
    ASSERT_TRUE(content.find("<<<<<<<") != std::string::npos);
    ASSERT_TRUE(content.find("=======") != std::string::npos);
    ASSERT_TRUE(content.find(">>>>>>>") != std::string::npos);

    fs::remove_all(temp_dir, ec);
}

TEST_CASE(CherryPick, FileDeletionTransplantation)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_cp_delete_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    stage_and_commit(temp_dir, {{"keep.txt", "Keep me\n"}, {"obsolete.txt", "Delete me\n"}}, {}, "Add files");

    create_branch(temp_dir, "cleanup");
    switch_branch(temp_dir, "cleanup");
    stage_and_commit(temp_dir, {}, {"obsolete.txt"}, "Remove obsolete file");

    switch_branch(temp_dir, "main");
    ASSERT_TRUE(fs::exists(temp_dir / "obsolete.txt"));

    CherryPickResult result = perform_cherry_pick(temp_dir, "cleanup");
    ASSERT_TRUE(result.success);
    ASSERT_FALSE(result.conflict);

    // obsolete.txt must be removed from working tree and index
    ASSERT_FALSE(fs::exists(temp_dir / "obsolete.txt"));
    {
        Index idx(temp_dir / ".minigit" / "index");
        ASSERT_FALSE(idx.entries().count("obsolete.txt") > 0);
        ASSERT_TRUE(idx.entries().count("keep.txt") > 0);
    }

    fs::remove_all(temp_dir, ec);
}
