#include "test_framework.h"
#include "merge/rebase.h"
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

void stage_and_commit(const fs::path& repo_root,
                      const std::vector<std::pair<std::string, std::string>>& files_to_write,
                      const std::vector<std::string>& files_to_delete,
                      const std::string& message,
                      const std::string& author = "MiniGit Tester <tester@minigit.org>")
{
    const fs::path git_dir = repo_root / ".minigit";
    ObjectDatabase db(git_dir / "objects");
    Index index(git_dir / "index");

    for (const auto& [rel_path, content] : files_to_write)
    {
        write_file(repo_root / rel_path, content);
        Blob b(content);
        db.write(b.id(), b.serialized());
        index.add(rel_path, b.id());
    }

    for (const auto& rel_path : files_to_delete)
    {
        std::error_code ec;
        fs::remove(repo_root / rel_path, ec);
        index.remove(rel_path);
    }
    index.write();

    std::vector<TreeEntry> entries;
    for (const auto& [path, id] : index.entries())
        entries.push_back({"100644", path, id});

    Tree tree(entries);
    db.write(tree.id(), tree.serialized());

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

        Index old_idx(git_dir / "index");
        for (const auto& [name, _] : old_idx.entries())
        {
            std::error_code ec;
            fs::remove(repo_root / name, ec);
        }

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
        current_sha = read_single_line(git_dir / head_raw.substr(5));
    else
        current_sha = head_raw;

    const fs::path branch_file = git_dir / "refs" / "heads" / branch_name;
    fs::create_directories(branch_file.parent_path());
    std::ofstream b_out(branch_file, std::ios::trunc);
    b_out << current_sha << '\n';
}

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE(Rebase, CleanReplayLinearHistory)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_rebase_clean_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    // 1. Initial commit A on main
    stage_and_commit(temp_dir, {{"fileA.txt", "Initial A\n"}}, {}, "Initial commit A on main");
    const std::string commit_a = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "main");

    // 2. Branch out to feature
    create_branch(temp_dir, "feature");

    // 3. Commit B on main
    stage_and_commit(temp_dir, {{"fileM.txt", "Main addition B\n"}}, {}, "Commit B on main");
    const std::string commit_b = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "main");

    // 4. Switch to feature, add commit C and commit D
    switch_branch(temp_dir, "feature");
    stage_and_commit(temp_dir, {{"feat1.txt", "Feature 1\n"}}, {}, "Commit C on feature", "Dev Alice <alice@feature.com>");
    stage_and_commit(temp_dir, {{"feat2.txt", "Feature 2\n"}}, {}, "Commit D on feature", "Dev Bob <bob@feature.com>");

    // 5. Rebase feature onto main
    RebaseResult res = perform_rebase(temp_dir, "main");

    ASSERT_TRUE(res.success);
    ASSERT_FALSE(res.conflict);
    ASSERT_FALSE(res.up_to_date);

    // 6. Verify working tree files
    ASSERT_TRUE(fs::exists(temp_dir / "fileA.txt"));
    ASSERT_TRUE(fs::exists(temp_dir / "fileM.txt"));
    ASSERT_TRUE(fs::exists(temp_dir / "feat1.txt"));
    ASSERT_TRUE(fs::exists(temp_dir / "feat2.txt"));

    ASSERT_EQ(read_file(temp_dir / "fileA.txt"), "Initial A\n");
    ASSERT_EQ(read_file(temp_dir / "fileM.txt"), "Main addition B\n");
    ASSERT_EQ(read_file(temp_dir / "feat1.txt"), "Feature 1\n");
    ASSERT_EQ(read_file(temp_dir / "feat2.txt"), "Feature 2\n");

    // 7. Verify commit lineage: D' -> C' -> B -> A
    ObjectDatabase db(temp_dir / ".minigit" / "objects");
    const std::string feat_head = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "feature");
    ASSERT_EQ(res.new_head_sha, feat_head);

    ParsedCommit d_prime = parse_commit(db.read(feat_head));
    ASSERT_EQ(d_prime.message, "Commit D on feature");
    ASSERT_EQ(d_prime.author, "Dev Bob <bob@feature.com>");
    ASSERT_EQ(d_prime.parent_ids.size(), 1);

    const std::string c_prime_sha = d_prime.parent_ids[0];
    ParsedCommit c_prime = parse_commit(db.read(c_prime_sha));
    ASSERT_EQ(c_prime.message, "Commit C on feature");
    ASSERT_EQ(c_prime.author, "Dev Alice <alice@feature.com>");
    ASSERT_EQ(c_prime.parent_ids.size(), 1);

    ASSERT_EQ(c_prime.parent_ids[0], commit_b);

    // 8. Verify HEAD points to refs/heads/feature
    const std::string head_ref = read_single_line(temp_dir / ".minigit" / "HEAD");
    ASSERT_EQ(head_ref, "ref: refs/heads/feature");

    // 9. Verify rebase-apply directory cleaned up
    ASSERT_FALSE(fs::exists(temp_dir / ".minigit" / "rebase-apply"));

    fs::remove_all(temp_dir, ec);
}

TEST_CASE(Rebase, UpToDateCheck)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_rebase_uptodate_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    stage_and_commit(temp_dir, {{"root.txt", "root\n"}}, {}, "Initial commit");
    create_branch(temp_dir, "feature");
    switch_branch(temp_dir, "feature");

    // Feature and main are at the exact same commit
    RebaseResult res = perform_rebase(temp_dir, "main");
    ASSERT_TRUE(res.success);
    ASSERT_TRUE(res.up_to_date);

    // Now commit on feature; main stays at root. Feature is already ahead of main.
    stage_and_commit(temp_dir, {{"feat.txt", "feat\n"}}, {}, "Feature commit");
    RebaseResult res2 = perform_rebase(temp_dir, "main");
    ASSERT_TRUE(res2.success);
    ASSERT_TRUE(res2.up_to_date);

    fs::remove_all(temp_dir, ec);
}

TEST_CASE(Rebase, FastForwardRebase)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_rebase_ff_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    stage_and_commit(temp_dir, {{"file.txt", "v1\n"}}, {}, "Commit 1 on main");
    create_branch(temp_dir, "feature");

    // Main advances to v2
    stage_and_commit(temp_dir, {{"file.txt", "v2\n"}}, {}, "Commit 2 on main");
    const std::string main_head = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "main");

    // Switch to feature (which is at Commit 1) and rebase onto main
    switch_branch(temp_dir, "feature");
    RebaseResult res = perform_rebase(temp_dir, "main");

    ASSERT_TRUE(res.success);
    ASSERT_TRUE(res.fast_forward);
    ASSERT_EQ(res.new_head_sha, main_head);

    const std::string feat_head = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "feature");
    ASSERT_EQ(feat_head, main_head);
    ASSERT_EQ(read_file(temp_dir / "file.txt"), "v2\n");

    fs::remove_all(temp_dir, ec);
}

TEST_CASE(Rebase, ConflictAndAbort)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_rebase_abort_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    stage_and_commit(temp_dir, {{"conflict.txt", "Line Base\n"}}, {}, "Initial commit");
    create_branch(temp_dir, "feature");

    // Commit on main modifying line
    stage_and_commit(temp_dir, {{"conflict.txt", "Line Main\n"}}, {}, "Main conflicting change");

    // Commit on feature modifying same line differently
    switch_branch(temp_dir, "feature");
    stage_and_commit(temp_dir, {{"conflict.txt", "Line Feature\n"}}, {}, "Feature conflicting change");
    const std::string feat_orig_sha = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "feature");

    // Rebase should detect conflict
    RebaseResult res = perform_rebase(temp_dir, "main");
    ASSERT_FALSE(res.success);
    ASSERT_TRUE(res.conflict);
    ASSERT_TRUE(fs::exists(temp_dir / ".minigit" / "rebase-apply"));

    // Verify conflict markers in working tree
    const std::string file_content = read_file(temp_dir / "conflict.txt");
    ASSERT_TRUE(file_content.find("<<<<<<<") != std::string::npos);
    ASSERT_TRUE(file_content.find(">>>>>>>") != std::string::npos);

    // Abort rebase
    RebaseResult abort_res = rebase_abort(temp_dir);
    ASSERT_TRUE(abort_res.success);
    ASSERT_TRUE(abort_res.aborted);
    ASSERT_FALSE(fs::exists(temp_dir / ".minigit" / "rebase-apply"));

    // Verify working tree restored to feature state
    ASSERT_EQ(read_file(temp_dir / "conflict.txt"), "Line Feature\n");
    const std::string feat_after_abort = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "feature");
    ASSERT_EQ(feat_after_abort, feat_orig_sha);

    const std::string head_ref = read_single_line(temp_dir / ".minigit" / "HEAD");
    ASSERT_EQ(head_ref, "ref: refs/heads/feature");

    fs::remove_all(temp_dir, ec);
}

TEST_CASE(Rebase, ConflictResolveAndContinue)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_rebase_continue_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    stage_and_commit(temp_dir, {{"data.txt", "Version 1\n"}}, {}, "Initial commit");
    create_branch(temp_dir, "feature");

    // Main updates data.txt
    stage_and_commit(temp_dir, {{"data.txt", "Version Main\n"}}, {}, "Main update");

    // Feature updates data.txt AND adds second commit with extra.txt
    switch_branch(temp_dir, "feature");
    stage_and_commit(temp_dir, {{"data.txt", "Version Feature\n"}}, {}, "Feature update data");
    stage_and_commit(temp_dir, {{"extra.txt", "Extra content\n"}}, {}, "Feature extra commit");

    // Rebase feature onto main -> conflicts on commit 1
    RebaseResult res = perform_rebase(temp_dir, "main");
    ASSERT_FALSE(res.success);
    ASSERT_TRUE(res.conflict);
    ASSERT_TRUE(fs::exists(temp_dir / ".minigit" / "rebase-apply"));

    // Resolve conflict in data.txt
    write_file(temp_dir / "data.txt", "Version Resolved\n");

    // Stage resolved file
    {
        ObjectDatabase db(temp_dir / ".minigit" / "objects");
        Blob b("Version Resolved\n");
        db.write(b.id(), b.serialized());
        Index idx(temp_dir / ".minigit" / "index");
        idx.add("data.txt", b.id());
        idx.write();
    }

    // Continue rebase
    RebaseResult cont_res = rebase_continue(temp_dir);
    ASSERT_TRUE(cont_res.success);
    ASSERT_FALSE(cont_res.conflict);
    ASSERT_FALSE(fs::exists(temp_dir / ".minigit" / "rebase-apply"));

    // Verify both files exist with correct contents
    ASSERT_EQ(read_file(temp_dir / "data.txt"), "Version Resolved\n");
    ASSERT_EQ(read_file(temp_dir / "extra.txt"), "Extra content\n");

    // Check lineage: two rebased commits on top of main
    ObjectDatabase db(temp_dir / ".minigit" / "objects");
    const std::string feat_head = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "feature");
    ParsedCommit commit_extra = parse_commit(db.read(feat_head));
    ASSERT_EQ(commit_extra.message, "Feature extra commit");

    ParsedCommit commit_resolved = parse_commit(db.read(commit_extra.parent_ids[0]));
    ASSERT_EQ(commit_resolved.message, "Feature update data");

    const std::string main_head = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "main");
    ASSERT_EQ(commit_resolved.parent_ids[0], main_head);

    fs::remove_all(temp_dir, ec);
}

TEST_CASE(Rebase, SkipConflictedCommit)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_rebase_skip_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    stage_and_commit(temp_dir, {{"conflict.txt", "Initial\n"}}, {}, "Initial commit");
    create_branch(temp_dir, "feature");

    // Main updates conflict.txt
    stage_and_commit(temp_dir, {{"conflict.txt", "Main Version\n"}}, {}, "Main update");

    // Feature updates conflict.txt (commit 1) and adds new_file.txt (commit 2)
    switch_branch(temp_dir, "feature");
    stage_and_commit(temp_dir, {{"conflict.txt", "Feature Version\n"}}, {}, "Feature conflict commit");
    stage_and_commit(temp_dir, {{"new_file.txt", "New File Content\n"}}, {}, "Feature clean commit");

    // Rebase onto main -> conflicts on commit 1
    RebaseResult res = perform_rebase(temp_dir, "main");
    ASSERT_TRUE(res.conflict);

    // Skip the conflicted commit
    RebaseResult skip_res = rebase_skip(temp_dir);
    ASSERT_TRUE(skip_res.success);
    ASSERT_FALSE(skip_res.conflict);
    ASSERT_FALSE(fs::exists(temp_dir / ".minigit" / "rebase-apply"));

    // Working tree should keep main's version of conflict.txt and have new_file.txt
    ASSERT_EQ(read_file(temp_dir / "conflict.txt"), "Main Version\n");
    ASSERT_EQ(read_file(temp_dir / "new_file.txt"), "New File Content\n");

    // Only 1 commit replayed on top of main
    ObjectDatabase db(temp_dir / ".minigit" / "objects");
    const std::string feat_head = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "feature");
    ParsedCommit c = parse_commit(db.read(feat_head));
    ASSERT_EQ(c.message, "Feature clean commit");

    const std::string main_head = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "main");
    ASSERT_EQ(c.parent_ids[0], main_head);

    fs::remove_all(temp_dir, ec);
}

TEST_CASE(Rebase, OntoSpecification)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_rebase_onto_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    // 1. Root commit on master
    stage_and_commit(temp_dir, {{"base.txt", "base\n"}}, {}, "Root commit");

    // 2. Branch topic1 from master
    create_branch(temp_dir, "topic1");
    switch_branch(temp_dir, "topic1");
    stage_and_commit(temp_dir, {{"topic1.txt", "topic1\n"}}, {}, "Topic 1 commit");

    // 3. Branch topic2 from topic1
    create_branch(temp_dir, "topic2");
    switch_branch(temp_dir, "topic2");
    stage_and_commit(temp_dir, {{"topic2.txt", "topic2\n"}}, {}, "Topic 2 commit");

    // 4. Commit on master
    switch_branch(temp_dir, "main");
    stage_and_commit(temp_dir, {{"main_new.txt", "main\n"}}, {}, "Main new commit");
    const std::string main_head = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "main");

    // 5. Rebase topic2 onto main specifying topic1 as upstream
    switch_branch(temp_dir, "topic2");
    RebaseResult res = perform_rebase(temp_dir, "topic1", "main");

    ASSERT_TRUE(res.success);
    ASSERT_FALSE(res.conflict);

    // In topic2: base.txt, main_new.txt, topic2.txt should exist. topic1.txt should NOT exist!
    ASSERT_TRUE(fs::exists(temp_dir / "base.txt"));
    ASSERT_TRUE(fs::exists(temp_dir / "main_new.txt"));
    ASSERT_TRUE(fs::exists(temp_dir / "topic2.txt"));
    ASSERT_FALSE(fs::exists(temp_dir / "topic1.txt"));

    // Parent of topic2 commit must be main_head!
    ObjectDatabase db(temp_dir / ".minigit" / "objects");
    const std::string topic2_head = read_single_line(temp_dir / ".minigit" / "refs" / "heads" / "topic2");
    ParsedCommit c = parse_commit(db.read(topic2_head));
    ASSERT_EQ(c.parent_ids[0], main_head);

    fs::remove_all(temp_dir, ec);
}

TEST_CASE(Rebase, DirtyWorkingTreeProtection)
{
    const auto temp_dir = fs::temp_directory_path() / "minigit_rebase_dirty_test";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    stage_and_commit(temp_dir, {{"tracked.txt", "Original\n"}}, {}, "Initial commit");
    create_branch(temp_dir, "feature");

    // Main advances
    stage_and_commit(temp_dir, {{"other.txt", "Other\n"}}, {}, "Main commit");

    switch_branch(temp_dir, "feature");

    // Modify tracked.txt in working tree without staging/committing
    write_file(temp_dir / "tracked.txt", "Dirty modification\n");

    RebaseResult res = perform_rebase(temp_dir, "main");
    ASSERT_FALSE(res.success);
    ASSERT_TRUE(res.error_message.find("unstaged changes") != std::string::npos);

    fs::remove_all(temp_dir, ec);
}
