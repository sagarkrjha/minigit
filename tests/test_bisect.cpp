#include "test_framework.h"
#include "bisect/bisect.h"
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
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace
{

struct TestDirGuard
{
    fs::path path;
    explicit TestDirGuard(const std::string &name)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("minigit_bisect_test_" + name + "_" + std::to_string(now));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TestDirGuard()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write_file(const fs::path &p, const std::string &content)
{
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string read_file(const fs::path &p)
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

std::string trim_str(std::string s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s;
}

std::string stage_and_commit(const fs::path &repo_root,
                            const std::vector<std::pair<std::string, std::string>> &files,
                            const std::string &message,
                            const std::vector<std::string> &extra_parents = {})
{
    const fs::path git_dir = repo_root / ".minigit";
    ObjectDatabase db(git_dir / "objects");
    Index index(git_dir / "index");

    for (const auto &[rel, content] : files)
    {
        write_file(repo_root / rel, content);
        Blob b(content);
        db.write(b.id(), b.serialized());
        index.add(rel, b.id());
    }
    index.write();

    std::vector<TreeEntry> entries;
    for (const auto &[path, id] : index.entries())
        entries.push_back({"100644", path, id});

    Tree tree(entries);
    db.write(tree.id(), tree.serialized());

    std::vector<std::string> parents;
    fs::path head_path = git_dir / "HEAD";
    std::string head_raw;
    {
        std::ifstream hf(head_path);
        std::getline(hf, head_raw);
    }
    head_raw = trim_str(head_raw);

    fs::path ref_path = head_path;
    if (head_raw.rfind("ref: ", 0) == 0)
    {
        ref_path = git_dir / head_raw.substr(5);
        if (fs::exists(ref_path))
        {
            std::ifstream rf(ref_path);
            std::string parent_sha;
            std::getline(rf, parent_sha);
            parent_sha = trim_str(parent_sha);
            if (!parent_sha.empty())
                parents.push_back(parent_sha);
        }
    }
    else if (!head_raw.empty())
    {
        parents.push_back(head_raw);
    }

    for (const auto &p : extra_parents)
        parents.push_back(p);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    Commit c(tree.id(), parents, "Tester <tester@minigit.org>", message);
    db.write(c.id(), c.serialized());

    fs::create_directories(ref_path.parent_path());
    std::ofstream rf(ref_path, std::ios::trunc);
    rf << c.id() << '\n';

    return c.id();
}

} // namespace

TEST_CASE(Bisect, LinearSearchFindsFirstBad)
{
    TestDirGuard guard("linear");
    const fs::path repo_dir = guard.path;
    Repository repo(repo_dir);
    repo.init();

    // Create a linear history of 6 commits:
    // C0 (good): v=0
    // C1 (good): v=1
    // C2 (good): v=2
    // C3 (bad! introduces bug): v=999
    // C4 (bad): v=1000
    // C5 (bad): v=1001
    std::string c0 = stage_and_commit(repo_dir, {{"app.txt", "v=0\n"}}, "Commit 0");
    std::string c1 = stage_and_commit(repo_dir, {{"app.txt", "v=1\n"}}, "Commit 1");
    std::string c2 = stage_and_commit(repo_dir, {{"app.txt", "v=2\n"}}, "Commit 2");
    std::string c3 = stage_and_commit(repo_dir, {{"app.txt", "v=999\n"}}, "Introduce bug in commit 3");
    std::string c4 = stage_and_commit(repo_dir, {{"app.txt", "v=1000\n"}}, "Commit 4");
    std::string c5 = stage_and_commit(repo_dir, {{"app.txt", "v=1001\n"}}, "Commit 5");

    const auto orig_cwd = fs::current_path();
    fs::current_path(repo_dir);

    // 1. Start bisect with bad=C5, good=C0
    const char *argv_start[] = {"minigit", "bisect", "start", c5.c_str(), c0.c_str()};
    int ret = minigit::bisect::bisect_command(5, argv_start);
    ASSERT_EQ(ret, 0);

    // Bisection should check out a midpoint commit (e.g. C2 or C3)
    // Loop until bisect finishes
    for (int step = 0; step < 5; ++step)
    {
        std::string content = read_file(repo_dir / "app.txt");
        if (content.find("v=999") != std::string::npos ||
            content.find("v=1000") != std::string::npos ||
            content.find("v=1001") != std::string::npos)
        {
            // Bad commit
            const char *argv_bad[] = {"minigit", "bisect", "bad"};
            int res = minigit::bisect::bisect_command(3, argv_bad);
            ASSERT_EQ(res, 0);
        }
        else
        {
            // Good commit
            const char *argv_good[] = {"minigit", "bisect", "good"};
            int res = minigit::bisect::bisect_command(3, argv_good);
            ASSERT_EQ(res, 0);
        }

        // Check if bad commit was isolated
        std::string cur_bad = read_file(repo_dir / ".minigit" / "refs" / "bisect" / "bad");
        cur_bad = trim_str(cur_bad);
        if (cur_bad == c3)
        {
            break;
        }
    }

    // Reset bisect session
    const char *argv_reset[] = {"minigit", "bisect", "reset"};
    ret = minigit::bisect::bisect_command(3, argv_reset);
    ASSERT_EQ(ret, 0);

    // Verify HEAD is back to main and files are restored
    std::string head = Repository::resolve_head_from_dir(repo_dir / ".minigit");
    ASSERT_EQ(head, c5);
    ASSERT_EQ(read_file(repo_dir / "app.txt"), "v=1001\n");
    ASSERT_FALSE(fs::exists(repo_dir / ".minigit" / "BISECT_START"));

    fs::current_path(orig_cwd);
}

TEST_CASE(Bisect, StepProgressionAndStatus)
{
    TestDirGuard guard("step_prog");
    const fs::path repo_dir = guard.path;
    Repository repo(repo_dir);
    repo.init();

    std::string c0 = stage_and_commit(repo_dir, {{"data.txt", "initial"}}, "C0");
    std::string c1 = stage_and_commit(repo_dir, {{"data.txt", "changed"}}, "C1");

    const auto orig_cwd = fs::current_path();
    fs::current_path(repo_dir);

    // 1. Start without arguments
    const char *argv_start[] = {"minigit", "bisect", "start"};
    int ret = minigit::bisect::bisect_command(3, argv_start);
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(fs::exists(repo_dir / ".minigit" / "BISECT_START"));

    // 2. Mark bad commit
    const char *argv_bad[] = {"minigit", "bisect", "bad", c1.c_str()};
    ret = minigit::bisect::bisect_command(4, argv_bad);
    ASSERT_EQ(ret, 0);

    // 3. Mark good commit
    const char *argv_good[] = {"minigit", "bisect", "good", c0.c_str()};
    ret = minigit::bisect::bisect_command(4, argv_good);
    ASSERT_EQ(ret, 0);

    // Since candidate set is only C1, C1 is immediately identified as first bad commit!
    std::string bad_commit = read_file(repo_dir / ".minigit" / "refs" / "bisect" / "bad");
    ASSERT_EQ(trim_str(bad_commit), c1);

    // 4. Reset
    const char *argv_reset[] = {"minigit", "bisect", "reset"};
    ret = minigit::bisect::bisect_command(3, argv_reset);
    ASSERT_EQ(ret, 0);

    fs::current_path(orig_cwd);
}

TEST_CASE(Bisect, SkipUntestableCommit)
{
    TestDirGuard guard("skip");
    const fs::path repo_dir = guard.path;
    Repository repo(repo_dir);
    repo.init();

    std::string c0 = stage_and_commit(repo_dir, {{"f.txt", "0"}}, "C0");
    std::string c1 = stage_and_commit(repo_dir, {{"f.txt", "1"}}, "C1");
    std::string c2 = stage_and_commit(repo_dir, {{"f.txt", "broken_build"}}, "C2");
    std::string c3 = stage_and_commit(repo_dir, {{"f.txt", "3"}}, "C3");

    const auto orig_cwd = fs::current_path();
    fs::current_path(repo_dir);

    const char *argv_start[] = {"minigit", "bisect", "start", c3.c_str(), c0.c_str()};
    ASSERT_EQ(minigit::bisect::bisect_command(5, argv_start), 0);

    // Skip current midpoint
    const char *argv_skip[] = {"minigit", "bisect", "skip"};
    int ret = minigit::bisect::bisect_command(3, argv_skip);
    ASSERT_EQ(ret, 0);

    // Verify skip marker exists
    fs::path refs_dir = repo_dir / ".minigit" / "refs" / "bisect";
    bool found_skip = false;
    for (const auto &entry : fs::directory_iterator(refs_dir))
    {
        if (entry.path().filename().string().starts_with("skip-"))
            found_skip = true;
    }
    ASSERT_TRUE(found_skip);

    const char *argv_reset[] = {"minigit", "bisect", "reset"};
    ASSERT_EQ(minigit::bisect::bisect_command(3, argv_reset), 0);

    fs::current_path(orig_cwd);
}

TEST_CASE(Bisect, TermsCustomization)
{
    TestDirGuard guard("terms");
    const fs::path repo_dir = guard.path;
    Repository repo(repo_dir);
    repo.init();

    std::string c0 = stage_and_commit(repo_dir, {{"f.txt", "1"}}, "C0");

    const auto orig_cwd = fs::current_path();
    fs::current_path(repo_dir);

    const char *argv_start[] = {"minigit", "bisect", "start"};
    ASSERT_EQ(minigit::bisect::bisect_command(3, argv_start), 0);

    // Set custom terms
    const char *argv_terms[] = {"minigit", "bisect", "terms", "--term-bad", "broken", "--term-good", "fixed"};
    int ret = minigit::bisect::bisect_command(7, argv_terms);
    ASSERT_EQ(ret, 0);

    // Verify terms stored in file
    std::string terms_content = read_file(repo_dir / ".minigit" / "BISECT_TERMS");
    ASSERT_TRUE(terms_content.find("broken") != std::string::npos);
    ASSERT_TRUE(terms_content.find("fixed") != std::string::npos);

    const char *argv_reset[] = {"minigit", "bisect", "reset"};
    ASSERT_EQ(minigit::bisect::bisect_command(3, argv_reset), 0);

    fs::current_path(orig_cwd);
}

TEST_CASE(Bisect, LogAndReplay)
{
    TestDirGuard guard("log_replay");
    const fs::path repo_dir = guard.path;
    Repository repo(repo_dir);
    repo.init();

    std::string c0 = stage_and_commit(repo_dir, {{"test.txt", "pass"}}, "C0");
    std::string c1 = stage_and_commit(repo_dir, {{"test.txt", "pass"}}, "C1");
    std::string c2 = stage_and_commit(repo_dir, {{"test.txt", "fail"}}, "C2");

    const auto orig_cwd = fs::current_path();
    fs::current_path(repo_dir);

    // 1. Perform a manual bisect session
    const char *argv_start[] = {"minigit", "bisect", "start", c2.c_str(), c0.c_str()};
    ASSERT_EQ(minigit::bisect::bisect_command(5, argv_start), 0);

    // Check log file content
    fs::path log_path = repo_dir / ".minigit" / "BISECT_LOG";
    ASSERT_TRUE(fs::exists(log_path));
    std::string log_text = read_file(log_path);
    ASSERT_TRUE(log_text.find("git bisect start") != std::string::npos);
    ASSERT_TRUE(log_text.find(c2) != std::string::npos);
    ASSERT_TRUE(log_text.find(c0) != std::string::npos);

    // Save log copy for replay
    fs::path saved_log = repo_dir / "saved_bisect.log";
    write_file(saved_log, log_text);

    // Reset
    const char *argv_reset[] = {"minigit", "bisect", "reset"};
    ASSERT_EQ(minigit::bisect::bisect_command(3, argv_reset), 0);
    ASSERT_FALSE(fs::exists(log_path));

    // Replay log
    std::string log_str = saved_log.string();
    const char *argv_replay[] = {"minigit", "bisect", "replay", log_str.c_str()};
    int ret = minigit::bisect::bisect_command(4, argv_replay);
    ASSERT_EQ(ret, 0);

    // State restored!
    ASSERT_TRUE(fs::exists(log_path));

    const char *argv_reset2[] = {"minigit", "bisect", "reset"};
    ASSERT_EQ(minigit::bisect::bisect_command(3, argv_reset2), 0);

    fs::current_path(orig_cwd);
}

TEST_CASE(Bisect, InvertedSanityCheck)
{
    TestDirGuard guard("inverted");
    const fs::path repo_dir = guard.path;
    Repository repo(repo_dir);
    repo.init();

    std::string c0 = stage_and_commit(repo_dir, {{"f.txt", "1"}}, "C0");
    std::string c1 = stage_and_commit(repo_dir, {{"f.txt", "2"}}, "C1");

    const auto orig_cwd = fs::current_path();
    fs::current_path(repo_dir);

    // Invert bad and good: good=C1 (descendant), bad=C0 (ancestor)
    const char *argv_start[] = {"minigit", "bisect", "start", c0.c_str(), c1.c_str()};
    int ret = minigit::bisect::bisect_command(5, argv_start);
    // Should fail with exit code 1
    ASSERT_EQ(ret, 1);

    fs::current_path(orig_cwd);
}

TEST_CASE(Bisect, MultiBranchDag)
{
    TestDirGuard guard("dag");
    const fs::path repo_dir = guard.path;
    Repository repo(repo_dir);
    repo.init();

    // Create a DAG:
    // C0 -> C1 (main)
    //   \-> C2 -> C3 (feature: C3 introduces bug)
    // Merge C3 into main -> C4
    std::string c0 = stage_and_commit(repo_dir, {{"common.txt", "c0"}}, "Commit 0");
    std::string c1 = stage_and_commit(repo_dir, {{"main.txt", "main1"}}, "Commit 1 on main");

    // Checkout c0 to branch feature
    write_file(repo_dir / ".minigit" / "HEAD", c0 + "\n");
    std::string c2 = stage_and_commit(repo_dir, {{"feat.txt", "feat1"}}, "Commit 2 on feature");
    std::string c3 = stage_and_commit(repo_dir, {{"feat.txt", "feat_BUG"}}, "Commit 3 introduces bug on feature");

    // Merge C3 into C1
    write_file(repo_dir / ".minigit" / "HEAD", c1 + "\n");
    std::string c4 = stage_and_commit(repo_dir, {{"feat.txt", "feat_BUG"}}, "Merge feature into main", {c3});

    const auto orig_cwd = fs::current_path();
    fs::current_path(repo_dir);

    // Bisect between bad=C4 and good=C1
    // The bug must be in C3!
    const char *argv_start[] = {"minigit", "bisect", "start", c4.c_str(), c1.c_str()};
    int ret = minigit::bisect::bisect_command(5, argv_start);
    ASSERT_EQ(ret, 0);

    // Test steps
    for (int i = 0; i < 4; ++i)
    {
        std::string content = read_file(repo_dir / "feat.txt");
        if (content.find("feat_BUG") != std::string::npos)
        {
            const char *argv_bad[] = {"minigit", "bisect", "bad"};
            minigit::bisect::bisect_command(3, argv_bad);
        }
        else
        {
            const char *argv_good[] = {"minigit", "bisect", "good"};
            minigit::bisect::bisect_command(3, argv_good);
        }

        std::string bad_commit = read_file(repo_dir / ".minigit" / "refs" / "bisect" / "bad");
        if (trim_str(bad_commit) == c3)
            break;
    }

    std::string final_bad = read_file(repo_dir / ".minigit" / "refs" / "bisect" / "bad");
    ASSERT_EQ(trim_str(final_bad), c3);

    const char *argv_reset[] = {"minigit", "bisect", "reset"};
    ASSERT_EQ(minigit::bisect::bisect_command(3, argv_reset), 0);

    fs::current_path(orig_cwd);
}
