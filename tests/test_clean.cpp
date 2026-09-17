#include "test_framework.h"
#include "staging/clean.h"
#include "staging/index.h"
#include "storage/blob.h"
#include "storage/commit.h"
#include "storage/object_database.h"
#include "storage/tree.h"
#include "repository/repository.h"

#include <filesystem>
#include <fstream>
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

fs::path create_test_repo(const std::string& subfolder)
{
    const fs::path repo_dir = fs::temp_directory_path() / "minigit_clean_tests" / subfolder;
    std::error_code ec;
    fs::remove_all(repo_dir, ec);
    fs::create_directories(repo_dir);

    Repository repo(repo_dir);
    repo.init();
    return repo_dir;
}

void stage_file(const fs::path& repo_root, const std::string& rel_path, const std::string& content)
{
    write_file(repo_root / rel_path, content);
    Blob b(content);
    ObjectDatabase db(repo_root / ".minigit" / "objects");
    db.write(b.id(), b.serialized());
    Index index(repo_root / ".minigit" / "index");
    index.add(rel_path, b.id());
    index.write();
}

} // namespace

TEST_CASE(Clean, SafetyRefusesWithoutForceOrDryRun)
{
    const fs::path repo = create_test_repo("safety");
    write_file(repo / "temp.txt", "garbage");

    CleanOptions opt; // force = false, dry_run = false
    CleanResult res = perform_clean(repo, opt);

    ASSERT_FALSE(res.success);
    ASSERT_TRUE(res.error_message.find("refusing to clean") != std::string::npos);
    ASSERT_TRUE(fs::exists(repo / "temp.txt"));
}

TEST_CASE(Clean, DryRunDoesNotDelete)
{
    const fs::path repo = create_test_repo("dry_run");
    write_file(repo / "untracked1.txt", "content 1");
    write_file(repo / "untracked2.txt", "content 2");

    CleanOptions opt;
    opt.dry_run = true;
    CleanResult res = perform_clean(repo, opt);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.items_cleaned.size(), 2);
    ASSERT_TRUE(res.output.find("Would remove untracked1.txt") != std::string::npos);
    ASSERT_TRUE(res.output.find("Would remove untracked2.txt") != std::string::npos);

    // Files must still exist after dry-run
    ASSERT_TRUE(fs::exists(repo / "untracked1.txt"));
    ASSERT_TRUE(fs::exists(repo / "untracked2.txt"));
}

TEST_CASE(Clean, ForceRemovesUntrackedFiles)
{
    const fs::path repo = create_test_repo("force_files");
    stage_file(repo, "tracked.txt", "important tracked content");
    write_file(repo / "untracked.txt", "disposable content");

    CleanOptions opt;
    opt.force = true;
    CleanResult res = perform_clean(repo, opt);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.items_cleaned.size(), 1);
    ASSERT_EQ(res.items_cleaned[0], "untracked.txt");
    ASSERT_TRUE(res.output.find("Removing untracked.txt") != std::string::npos);

    // Untracked file removed, tracked file preserved
    ASSERT_FALSE(fs::exists(repo / "untracked.txt"));
    ASSERT_TRUE(fs::exists(repo / "tracked.txt"));
}

TEST_CASE(Clean, RemoveDirectoriesOptionD)
{
    const fs::path repo = create_test_repo("directories");
    stage_file(repo, "src/main.cpp", "int main() {}");

    // Untracked directory with untracked files
    write_file(repo / "build_output/sub/artifact.bin", "binary data");

    // 1. Without -d, directory as a whole is not removed
    CleanOptions opt1;
    opt1.force = true;
    opt1.remove_directories = false;
    CleanResult res1 = perform_clean(repo, opt1);
    ASSERT_TRUE(res1.success);
    // Files inside were removed, but directory remains
    ASSERT_FALSE(fs::exists(repo / "build_output/sub/artifact.bin"));

    // 2. Re-create untracked directory and run with -d
    write_file(repo / "temp_dir/extra.txt", "temp");
    CleanOptions opt2;
    opt2.force = true;
    opt2.remove_directories = true;
    CleanResult res2 = perform_clean(repo, opt2);
    ASSERT_TRUE(res2.success);
    ASSERT_TRUE(res2.output.find("Removing temp_dir/") != std::string::npos);
    ASSERT_FALSE(fs::exists(repo / "temp_dir"));

    // Ensure directory containing tracked file is never deleted
    ASSERT_TRUE(fs::exists(repo / "src"));
    ASSERT_TRUE(fs::exists(repo / "src/main.cpp"));
}

TEST_CASE(Clean, RespectsIgnoreRulesUnlessX)
{
    const fs::path repo = create_test_repo("ignored");
    write_file(repo / ".minigitignore", "*.log\nbuild/\n");
    write_file(repo / "debug.log", "log details");
    write_file(repo / "scratch.txt", "scratch note");

    // 1. Without -x: debug.log is ignored and preserved, scratch.txt is cleaned
    CleanOptions opt_no_x;
    opt_no_x.force = true;
    opt_no_x.include_ignored = false;
    CleanResult res1 = perform_clean(repo, opt_no_x);
    ASSERT_TRUE(res1.success);
    ASSERT_FALSE(fs::exists(repo / "scratch.txt"));
    ASSERT_TRUE(fs::exists(repo / "debug.log"));

    // 2. With -x: ignored files are also cleaned
    CleanOptions opt_x;
    opt_x.force = true;
    opt_x.include_ignored = true;
    CleanResult res2 = perform_clean(repo, opt_x);
    ASSERT_TRUE(res2.success);
    ASSERT_FALSE(fs::exists(repo / "debug.log"));
}

TEST_CASE(Clean, PathFilter)
{
    const fs::path repo = create_test_repo("path_filter");
    write_file(repo / "src/temp.cpp", "junk");
    write_file(repo / "docs/notes.tmp", "junk");

    CleanOptions opt;
    opt.force = true;
    opt.paths = {"src"};

    CleanResult res = perform_clean(repo, opt);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.items_cleaned.size(), 1);
    ASSERT_EQ(res.items_cleaned[0], "src/temp.cpp");

    ASSERT_FALSE(fs::exists(repo / "src/temp.cpp"));
    ASSERT_TRUE(fs::exists(repo / "docs/notes.tmp"));
}
