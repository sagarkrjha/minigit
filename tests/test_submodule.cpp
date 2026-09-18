#include "test_framework.h"
#include "submodule/submodule.h"
#include "submodule/submodule_config.h"
#include "repository/repository.h"
#include "repository/init.h"
#include "staging/add.h"
#include "history/commit.h"
#include "storage/ls_tree.h"
#include "staging/index.h"
#include "diff/diff.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
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
        path = fs::temp_directory_path() / ("minigit_submodule_test_" + name + "_" + std::to_string(now));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TestDirGuard()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write_file(const fs::path &path, const std::string &content)
{
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string read_file(const fs::path &path)
{
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

TEST_CASE(Submodule, ConfigParseAndSave)
{
    TestDirGuard guard("config");
    const fs::path config_file = guard.path / ".minigitmodules";

    {
        SubmoduleConfig cfg(config_file);
        cfg.add_or_update({"libs/engine", "libs/engine", "https://example.com/engine.git", "main"});
        cfg.add_or_update({"libs/audio", "libs/audio", "https://example.com/audio.git", ""});
        cfg.save();
    }

    ASSERT_TRUE(fs::exists(config_file));

    {
        SubmoduleConfig cfg(config_file);
        ASSERT_EQ(cfg.entries().size(), 2);

        const auto *e1 = cfg.find_by_name("libs/engine");
        ASSERT_TRUE(e1 != nullptr);
        ASSERT_EQ(e1->path, "libs/engine");
        ASSERT_EQ(e1->url, "https://example.com/engine.git");
        ASSERT_EQ(e1->branch, "main");

        const auto *e2 = cfg.find_by_path("libs/audio");
        ASSERT_TRUE(e2 != nullptr);
        ASSERT_EQ(e2->name, "libs/audio");
        ASSERT_EQ(e2->url, "https://example.com/audio.git");

        ASSERT_TRUE(cfg.remove("libs/audio"));
        ASSERT_EQ(cfg.entries().size(), 1);
        cfg.save();
    }

    {
        SubmoduleConfig cfg(config_file);
        ASSERT_EQ(cfg.entries().size(), 1);
        ASSERT_TRUE(cfg.find_by_name("libs/audio") == nullptr);
    }
}

TEST_CASE(Submodule, AddAndTreeGitlink)
{
    TestDirGuard guard("add_gitlink");
    const fs::path origin_child = guard.path / "child_repo";
    const fs::path parent_repo = guard.path / "parent_repo";

    // 1. Setup child repository
    fs::create_directories(origin_child);
    Repository(origin_child).init();
    write_file(origin_child / "child_file.txt", "hello from child");
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(origin_child);
        add_files({"child_file.txt"});
        commit("Initial child commit", "Author <author@example.com>");
        fs::current_path(old_cwd);
    }

    // 2. Setup parent repository
    fs::create_directories(parent_repo);
    Repository(parent_repo).init();
    write_file(parent_repo / "parent_file.txt", "hello from parent");
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(parent_repo);
        add_files({"parent_file.txt"});
        commit("Initial parent commit", "Author <author@example.com>");

        // Add submodule
        std::string child_url = origin_child.string();
        const char *argv[] = {"minigit", "submodule", "add", child_url.c_str(), "libs/child"};
        int res = submodule_command(5, argv);
        ASSERT_EQ(res, 0);

        // Verify .minigitmodules exists
        ASSERT_TRUE(fs::exists(parent_repo / ".minigitmodules"));

        // Verify child working tree populated
        ASSERT_TRUE(fs::exists(parent_repo / "libs/child/child_file.txt"));
        ASSERT_EQ(read_file(parent_repo / "libs/child/child_file.txt"), "hello from child");

        // Verify .minigit file in child working tree
        ASSERT_TRUE(fs::is_regular_file(parent_repo / "libs/child/.minigit"));

        // Verify index has gitlink
        Index idx(parent_repo / ".minigit/index");
        ASSERT_TRUE(idx.entries().count("libs/child") > 0);
        ASSERT_TRUE(idx.entries().count(".minigitmodules") > 0);

        // Commit in parent
        commit("Add child submodule", "Author <author@example.com>");

        // Verify ls-tree output contains 160000 commit
        LsTreeOptions opts;
        opts.tree_ish = "HEAD";
        auto ls_res = perform_ls_tree(parent_repo, opts);
        ASSERT_TRUE(ls_res.success);

        bool found_gitlink = false;
        for (const auto &e : ls_res.entries)
        {
            if (e.path == "libs/child")
            {
                ASSERT_EQ(e.mode, "160000");
                ASSERT_EQ(e.type, "commit");
                found_gitlink = true;
            }
        }
        ASSERT_TRUE(found_gitlink);

        fs::current_path(old_cwd);
    }
}

TEST_CASE(Submodule, StatusAndNewCommits)
{
    TestDirGuard guard("status");
    const fs::path origin_child = guard.path / "child_repo";
    const fs::path parent_repo = guard.path / "parent_repo";

    fs::create_directories(origin_child);
    Repository(origin_child).init();
    write_file(origin_child / "lib.txt", "v1");
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(origin_child);
        add_files({"lib.txt"});
        commit("Initial lib", "Author <author@example.com>");
        fs::current_path(old_cwd);
    }

    fs::create_directories(parent_repo);
    Repository(parent_repo).init();
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(parent_repo);

        std::string child_url = origin_child.string();
        const char *argv[] = {"minigit", "submodule", "add", child_url.c_str(), "sub/lib"};
        ASSERT_EQ(submodule_command(5, argv), 0);
        commit("Add sub/lib", "Author <author@example.com>");

        // Status should succeed cleanly
        const char *st_argv[] = {"minigit", "submodule", "status"};
        ASSERT_EQ(submodule_command(3, st_argv), 0);

        // Make new commit in submodule
        fs::current_path(parent_repo / "sub/lib");
        write_file(parent_repo / "sub/lib/lib.txt", "v2");
        add_files({"lib.txt"});
        commit("Update lib to v2", "Author <author@example.com>");

        // Return to parent
        fs::current_path(parent_repo);

        // Stage the submodule's new commit
        add_files({"sub/lib"});

        Index idx(parent_repo / ".minigit/index");
        Repository sub_repo = Repository::discover(parent_repo / "sub/lib");
        std::string sub_head = Repository::resolve_head_from_dir(sub_repo.git_dir());
        ASSERT_EQ(idx.entries().at("sub/lib"), sub_head);

        fs::current_path(old_cwd);
    }
}

TEST_CASE(Submodule, InitAndUpdate)
{
    TestDirGuard guard("init_update");
    const fs::path origin_child = guard.path / "child_repo";
    const fs::path parent_repo = guard.path / "parent_repo";

    fs::create_directories(origin_child);
    Repository(origin_child).init();
    write_file(origin_child / "data.txt", "child data");
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(origin_child);
        add_files({"data.txt"});
        commit("Commit 1", "Author <author@example.com>");
        fs::current_path(old_cwd);
    }

    fs::create_directories(parent_repo);
    Repository(parent_repo).init();
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(parent_repo);

        std::string child_url = origin_child.string();
        const char *argv[] = {"minigit", "submodule", "add", child_url.c_str(), "deps/data"};
        ASSERT_EQ(submodule_command(5, argv), 0);
        commit("Add deps/data", "Author <author@example.com>");

        // Deinit submodule
        const char *deinit_argv[] = {"minigit", "submodule", "deinit", "--all"};
        ASSERT_EQ(submodule_command(4, deinit_argv), 0);

        // Working tree should be deleted
        ASSERT_FALSE(fs::exists(parent_repo / "deps/data/data.txt"));

        // Init and update
        const char *update_argv[] = {"minigit", "submodule", "update", "--init"};
        ASSERT_EQ(submodule_command(4, update_argv), 0);

        // Working tree restored
        ASSERT_TRUE(fs::exists(parent_repo / "deps/data/data.txt"));
        ASSERT_EQ(read_file(parent_repo / "deps/data/data.txt"), "child data");

        fs::current_path(old_cwd);
    }
}

TEST_CASE(Submodule, ForeachExecution)
{
    TestDirGuard guard("foreach");
    const fs::path child = guard.path / "child";
    const fs::path parent = guard.path / "parent";

    fs::create_directories(child);
    Repository(child).init();
    write_file(child / "f.txt", "foo");
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(child);
        add_files({"f.txt"});
        commit("C1", "A <a@b.com>");
        fs::current_path(old_cwd);
    }

    fs::create_directories(parent);
    Repository(parent).init();
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(parent);

        std::string child_url = child.string();
        const char *argv[] = {"minigit", "submodule", "add", child_url.c_str(), "sub"};
        ASSERT_EQ(submodule_command(5, argv), 0);

        const char *fe_argv[] = {"minigit", "submodule", "foreach", "echo", "submodule_ok"};
        ASSERT_EQ(submodule_command(5, fe_argv), 0);

        fs::current_path(old_cwd);
    }
}

TEST_CASE(Submodule, SyncRemoteUrl)
{
    TestDirGuard guard("sync");
    const fs::path child = guard.path / "child";
    const fs::path parent = guard.path / "parent";

    fs::create_directories(child);
    Repository(child).init();
    write_file(child / "f.txt", "foo");
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(child);
        add_files({"f.txt"});
        commit("C1", "A <a@b.com>");
        fs::current_path(old_cwd);
    }

    fs::create_directories(parent);
    Repository(parent).init();
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(parent);

        std::string child_url = child.string();
        const char *argv[] = {"minigit", "submodule", "add", child_url.c_str(), "sub"};
        ASSERT_EQ(submodule_command(5, argv), 0);

        // Update URL in .minigitmodules
        SubmoduleConfig cfg(parent / ".minigitmodules");
        cfg.add_or_update({"sub", "sub", "https://updated-url.com/child.git", "main"});
        cfg.save();

        const char *sync_argv[] = {"minigit", "submodule", "sync"};
        ASSERT_EQ(submodule_command(3, sync_argv), 0);

        // Verify config updated
        auto url_opt = SubmoduleConfig::get_config_url(parent / ".minigit", "sub");
        ASSERT_TRUE(url_opt.has_value());
        ASSERT_EQ(*url_opt, "https://updated-url.com/child.git");

        fs::current_path(old_cwd);
    }
}

TEST_CASE(Submodule, DeinitAndSummary)
{
    TestDirGuard guard("deinit_summary");
    const fs::path child = guard.path / "child";
    const fs::path parent = guard.path / "parent";

    fs::create_directories(child);
    Repository(child).init();
    write_file(child / "f.txt", "1");
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(child);
        add_files({"f.txt"});
        commit("C1", "A <a@b.com>");
        fs::current_path(old_cwd);
    }

    fs::create_directories(parent);
    Repository(parent).init();
    {
        const auto old_cwd = fs::current_path();
        fs::current_path(parent);

        std::string child_url = child.string();
        const char *argv[] = {"minigit", "submodule", "add", child_url.c_str(), "ext"};
        ASSERT_EQ(submodule_command(5, argv), 0);
        commit("Add ext", "Author <author@example.com>");

        // Summary command
        const char *summary_argv[] = {"minigit", "submodule", "summary"};
        ASSERT_EQ(submodule_command(3, summary_argv), 0);

        // Deinit command with specific path
        const char *deinit_argv[] = {"minigit", "submodule", "deinit", "ext"};
        ASSERT_EQ(submodule_command(4, deinit_argv), 0);
        ASSERT_FALSE(fs::exists(parent / "ext/f.txt"));

        // Status after deinit should show uninitialized
        const char *status_argv[] = {"minigit", "submodule", "status"};
        ASSERT_EQ(submodule_command(3, status_argv), 0);

        fs::current_path(old_cwd);
    }
}
