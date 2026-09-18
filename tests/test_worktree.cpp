#include "test_framework.h"
#include "worktree/worktree.h"
#include "repository/repository.h"
#include "storage/object_database.h"
#include "storage/blob.h"
#include "storage/tree.h"
#include "storage/commit.h"
#include "staging/index.h"
#include "core/file.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void write_file(const fs::path& p, const std::string& content)
{
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string create_commit(const fs::path& repo_root, const std::string& msg, const std::string& parent_sha = "")
{
    Repository repo(repo_root);
    Index index(repo.git_dir() / "index");
    ObjectDatabase db(repo.objects_dir());

    std::vector<TreeEntry> tree_entries;
    for (const auto& [path, blob_id] : index.entries())
    {
        tree_entries.push_back({"100644", path, blob_id});
    }
    Tree tree(std::move(tree_entries));
    db.write(tree.id(), tree.serialized());

    std::vector<std::string> parents;
    if (!parent_sha.empty())
        parents.push_back(parent_sha);

    Commit c(tree.id(), parents, "Test Author <author@minigit.test>", msg);
    db.write(c.id(), c.serialized());

    // Update branch ref
    std::string branch = "main";
    std::filesystem::path head_path = repo.git_dir() / "HEAD";
    std::ifstream hf(head_path);
    std::string hline;
    if (std::getline(hf, hline))
    {
        if (hline.rfind("ref: refs/heads/", 0) == 0)
            branch = hline.substr(16);
    }
    while (!branch.empty() && (branch.back() == '\r' || branch.back() == '\n' || branch.back() == ' '))
        branch.pop_back();

    std::filesystem::path ref_path = repo.refs_dir() / "heads" / branch;
    fs::create_directories(ref_path.parent_path());
    std::ofstream rf(ref_path, std::ios::trunc);
    rf << c.id() << '\n';

    return c.id();
}

void stage_test_file(const fs::path& repo_root, const std::string& rel_path, const std::string& content)
{
    write_file(repo_root / rel_path, content);
    Blob b(content);
    Repository repo(repo_root);
    ObjectDatabase db(repo.objects_dir());
    db.write(b.id(), b.serialized());

    Index index(repo.index_path());
    index.add(rel_path, b.id());
    index.write();
}

fs::path setup_test_repo(const std::string& subfolder)
{
    const fs::path base = fs::temp_directory_path() / "minigit_worktree_tests" / subfolder;
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base);

    const fs::path main_repo = base / "main";
    fs::create_directories(main_repo);
    Repository repo(main_repo);
    repo.init();

    stage_test_file(main_repo, "hello.txt", "Hello MiniGit Worktree!\n");
    create_commit(main_repo, "Initial commit");

    return base;
}

} // namespace

TEST_CASE(Worktree, AddWithNewBranch)
{
    const fs::path base = setup_test_repo("add_branch");
    const fs::path main_repo = base / "main";
    const fs::path wt_path = base / "wt1";

    std::string wt_str = wt_path.string();
    const char* argv[] = {"minigit", "worktree", "add", "-b", "feature-branch", wt_str.c_str()};
    const auto orig_cwd = fs::current_path();
    fs::current_path(main_repo);

    int ret = minigit::worktree::worktree_command(6, argv);
    fs::current_path(orig_cwd);

    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(fs::exists(wt_path));
    ASSERT_TRUE(fs::is_regular_file(wt_path / ".minigit"));
    ASSERT_TRUE(fs::exists(wt_path / "hello.txt"));

    ASSERT_EQ(read_file(wt_path / "hello.txt"), "Hello MiniGit Worktree!\n");

    Repository wt_repo = Repository::discover(wt_path);
    ASSERT_TRUE(wt_repo.is_worktree());
    ASSERT_EQ(wt_repo.worktree_name(), "wt1");
    ASSERT_TRUE(fs::equivalent(wt_repo.common_dir(), main_repo / ".minigit"));

    std::ifstream hf(wt_repo.head_path());
    std::string hline;
    std::getline(hf, hline);
    while (!hline.empty() && (hline.back() == '\r' || hline.back() == '\n' || hline.back() == ' '))
        hline.pop_back();
    ASSERT_EQ(hline, "ref: refs/heads/feature-branch");

    ASSERT_TRUE(fs::exists(main_repo / ".minigit" / "refs" / "heads" / "feature-branch"));
}

TEST_CASE(Worktree, AddDetachedHead)
{
    const fs::path base = setup_test_repo("add_detach");
    const fs::path main_repo = base / "main";
    const fs::path wt_path = base / "wt_detach";

    std::string wt_str = wt_path.string();
    const char* argv[] = {"minigit", "worktree", "add", "--detach", wt_str.c_str()};
    const auto orig_cwd = fs::current_path();
    fs::current_path(main_repo);

    int ret = minigit::worktree::worktree_command(5, argv);
    fs::current_path(orig_cwd);

    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(fs::exists(wt_path));
    ASSERT_TRUE(fs::exists(wt_path / "hello.txt"));

    Repository wt_repo = Repository::discover(wt_path);
    ASSERT_TRUE(wt_repo.is_worktree());

    auto worktrees = minigit::worktree::get_all_worktrees(Repository(main_repo));
    bool found_detached = false;
    for (const auto& wt : worktrees)
    {
        if (wt.id == "wt_detach")
        {
            ASSERT_TRUE(wt.is_detached);
            found_detached = true;
        }
    }
    ASSERT_TRUE(found_detached);
}

TEST_CASE(Worktree, ListAllWorktrees)
{
    const fs::path base = setup_test_repo("list_all");
    const fs::path main_repo = base / "main";
    const fs::path wt1 = base / "wt1";
    const fs::path wt2 = base / "wt2";

    std::string wt1_str = wt1.string();
    std::string wt2_str = wt2.string();

    const auto orig_cwd = fs::current_path();
    fs::current_path(main_repo);

    const char* argv1[] = {"minigit", "worktree", "add", "-b", "feat1", wt1_str.c_str()};
    minigit::worktree::worktree_command(6, argv1);

    const char* argv2[] = {"minigit", "worktree", "add", "-b", "feat2", wt2_str.c_str()};
    minigit::worktree::worktree_command(6, argv2);

    Repository repo = Repository::discover(main_repo);
    auto wts = minigit::worktree::get_all_worktrees(repo);
    fs::current_path(orig_cwd);

    ASSERT_EQ(wts.size(), 3); // main + wt1 + wt2
    ASSERT_TRUE(wts[0].is_main);
    ASSERT_EQ(wts[0].branch, "refs/heads/main");

    bool has_wt1 = false;
    bool has_wt2 = false;
    for (const auto& w : wts)
    {
        if (w.id == "wt1") has_wt1 = true;
        if (w.id == "wt2") has_wt2 = true;
    }
    ASSERT_TRUE(has_wt1);
    ASSERT_TRUE(has_wt2);
}

TEST_CASE(Worktree, CommitInsideLinkedWorktree)
{
    const fs::path base = setup_test_repo("commit_inside");
    const fs::path main_repo = base / "main";
    const fs::path wt_path = base / "wt_branch";

    std::string wt_str = wt_path.string();
    const auto orig_cwd = fs::current_path();
    fs::current_path(main_repo);

    const char* argv1[] = {"minigit", "worktree", "add", "-b", "branch-wt", wt_str.c_str()};
    minigit::worktree::worktree_command(6, argv1);

    // Now work in wt_path
    fs::current_path(wt_path);
    stage_test_file(wt_path, "feature.txt", "Feature content from linked worktree!\n");
    std::string commit_sha = create_commit(wt_path, "Feature commit");

    ASSERT_FALSE(commit_sha.empty());

    // Verify branch-wt in main_repo references new commit
    std::ifstream bf(main_repo / ".minigit" / "refs" / "heads" / "branch-wt");
    std::string bsha;
    std::getline(bf, bsha);
    while (!bsha.empty() && (bsha.back() == '\r' || bsha.back() == '\n' || bsha.back() == ' '))
        bsha.pop_back();
    ASSERT_EQ(bsha, commit_sha);

    // Verify main branch in main_repo is completely untouched
    std::ifstream mf(main_repo / ".minigit" / "refs" / "heads" / "main");
    std::string msha;
    std::getline(mf, msha);
    while (!msha.empty() && (msha.back() == '\r' || msha.back() == '\n' || msha.back() == ' '))
        msha.pop_back();
    ASSERT_NE(msha, commit_sha);

    // Verify main_repo object DB contains the commit
    ObjectDatabase main_db(main_repo / ".minigit" / "objects");
    ASSERT_TRUE(main_db.contains(commit_sha));

    fs::current_path(orig_cwd);
}

TEST_CASE(Worktree, BranchExclusivitySafeguard)
{
    const fs::path base = setup_test_repo("branch_exclusivity");
    const fs::path main_repo = base / "main";
    const fs::path wt_path = base / "wt_exclusive";

    std::string wt_str = wt_path.string();
    const auto orig_cwd = fs::current_path();
    fs::current_path(main_repo);

    const char* argv1[] = {"minigit", "worktree", "add", "-b", "active-feat", wt_str.c_str()};
    minigit::worktree::worktree_command(6, argv1);

    Repository repo = Repository::discover(main_repo);

    auto match = repo.find_branch_worktree("active-feat");
    ASSERT_TRUE(match.is_checked_out);
    ASSERT_FALSE(match.is_current_worktree);
    ASSERT_TRUE(fs::equivalent(match.worktree_path, wt_path));

    auto main_match = repo.find_branch_worktree("main");
    ASSERT_TRUE(main_match.is_checked_out);
    ASSERT_TRUE(main_match.is_current_worktree);

    const fs::path wt2_path = base / "wt_duplicate";
    std::string wt2_str = wt2_path.string();
    const char* argv2[] = {"minigit", "worktree", "add", wt2_str.c_str(), "active-feat"};
    int ret = minigit::worktree::worktree_command(5, argv2);
    ASSERT_NE(ret, 0);

    fs::current_path(orig_cwd);
}

TEST_CASE(Worktree, LockAndUnlock)
{
    const fs::path base = setup_test_repo("lock_unlock");
    const fs::path main_repo = base / "main";
    const fs::path wt_path = base / "wt_locked";

    std::string wt_str = wt_path.string();
    const auto orig_cwd = fs::current_path();
    fs::current_path(main_repo);

    const char* argv1[] = {"minigit", "worktree", "add", "-b", "lock-feat", wt_str.c_str()};
    minigit::worktree::worktree_command(6, argv1);

    const char* argv_lock[] = {"minigit", "worktree", "lock", "--reason", "on external drive", wt_str.c_str()};
    int ret_lock = minigit::worktree::worktree_command(6, argv_lock);
    ASSERT_EQ(ret_lock, 0);

    Repository repo = Repository::discover(main_repo);
    auto wts = minigit::worktree::get_all_worktrees(repo);
    bool is_locked = false;
    std::string reason;
    for (const auto& w : wts)
    {
        if (w.id == "wt_locked")
        {
            is_locked = w.is_locked;
            reason = w.lock_reason;
        }
    }
    ASSERT_TRUE(is_locked);
    ASSERT_EQ(reason, "on external drive");

    const char* argv_rm[] = {"minigit", "worktree", "remove", wt_str.c_str()};
    int ret_rm = minigit::worktree::worktree_command(4, argv_rm);
    ASSERT_NE(ret_rm, 0);

    const char* argv_unlock[] = {"minigit", "worktree", "unlock", wt_str.c_str()};
    int ret_unlock = minigit::worktree::worktree_command(4, argv_unlock);
    ASSERT_EQ(ret_unlock, 0);

    int ret_rm2 = minigit::worktree::worktree_command(4, argv_rm);
    ASSERT_EQ(ret_rm2, 0);
    ASSERT_FALSE(fs::exists(wt_path));

    fs::current_path(orig_cwd);
}

TEST_CASE(Worktree, RemoveDirtySafeguard)
{
    const fs::path base = setup_test_repo("remove_dirty");
    const fs::path main_repo = base / "main";
    const fs::path wt_path = base / "wt_dirty";

    std::string wt_str = wt_path.string();
    const auto orig_cwd = fs::current_path();
    fs::current_path(main_repo);

    const char* argv1[] = {"minigit", "worktree", "add", "-b", "dirty-feat", wt_str.c_str()};
    minigit::worktree::worktree_command(6, argv1);

    write_file(wt_path / "untracked.tmp", "some uncommitted work");

    const char* argv_rm[] = {"minigit", "worktree", "remove", wt_str.c_str()};
    int ret = minigit::worktree::worktree_command(4, argv_rm);
    ASSERT_NE(ret, 0);
    ASSERT_TRUE(fs::exists(wt_path));

    const char* argv_rm_f[] = {"minigit", "worktree", "remove", "--force", wt_str.c_str()};
    int ret_f = minigit::worktree::worktree_command(5, argv_rm_f);
    ASSERT_EQ(ret_f, 0);
    ASSERT_FALSE(fs::exists(wt_path));

    fs::current_path(orig_cwd);
}

TEST_CASE(Worktree, PruneOrphanedMetadata)
{
    const fs::path base = setup_test_repo("prune_orphans");
    const fs::path main_repo = base / "main";
    const fs::path wt_path = base / "wt_to_delete";

    std::string wt_str = wt_path.string();
    const auto orig_cwd = fs::current_path();
    fs::current_path(main_repo);

    const char* argv1[] = {"minigit", "worktree", "add", "-b", "prune-feat", wt_str.c_str()};
    minigit::worktree::worktree_command(6, argv1);

    std::error_code ec;
    fs::remove_all(wt_path, ec);
    ASSERT_FALSE(fs::exists(wt_path));

    Repository repo = Repository::discover(main_repo);
    auto wts = minigit::worktree::get_all_worktrees(repo);
    bool prunable_found = false;
    for (const auto& w : wts)
    {
        if (w.id == "wt_to_delete")
        {
            ASSERT_TRUE(w.is_prunable);
            prunable_found = true;
        }
    }
    ASSERT_TRUE(prunable_found);

    const char* argv_prune_n[] = {"minigit", "worktree", "prune", "-n"};
    minigit::worktree::worktree_command(4, argv_prune_n);
    ASSERT_TRUE(fs::exists(main_repo / ".minigit" / "worktrees" / "wt_to_delete"));

    const char* argv_prune[] = {"minigit", "worktree", "prune"};
    int ret = minigit::worktree::worktree_command(3, argv_prune);
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(fs::exists(main_repo / ".minigit" / "worktrees" / "wt_to_delete"));

    fs::current_path(orig_cwd);
}

TEST_CASE(Worktree, MoveWorkingTree)
{
    const fs::path base = setup_test_repo("move_wt");
    const fs::path main_repo = base / "main";
    const fs::path wt_old = base / "wt_old";
    const fs::path wt_new = base / "wt_new";

    std::string old_str = wt_old.string();
    std::string new_str = wt_new.string();

    const auto orig_cwd = fs::current_path();
    fs::current_path(main_repo);

    const char* argv1[] = {"minigit", "worktree", "add", "-b", "move-feat", old_str.c_str()};
    minigit::worktree::worktree_command(6, argv1);

    const char* argv_move[] = {"minigit", "worktree", "move", old_str.c_str(), new_str.c_str()};
    int ret = minigit::worktree::worktree_command(5, argv_move);
    ASSERT_EQ(ret, 0);

    ASSERT_FALSE(fs::exists(wt_old));
    ASSERT_TRUE(fs::exists(wt_new));
    ASSERT_TRUE(fs::exists(wt_new / "hello.txt"));

    Repository repo_new = Repository::discover(wt_new);
    ASSERT_TRUE(repo_new.is_worktree());
    ASSERT_TRUE(fs::equivalent(repo_new.root(), wt_new));

    fs::current_path(orig_cwd);
}
