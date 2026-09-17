#include "test_framework.h"
#include "staging/ls_files.h"
#include "storage/ls_tree.h"
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
    const fs::path repo_dir = fs::temp_directory_path() / "minigit_ls_tests" / subfolder;
    std::error_code ec;
    fs::remove_all(repo_dir, ec);
    fs::create_directories(repo_dir);

    Repository repo(repo_dir);
    repo.init();
    return repo_dir;
}

std::string stage_file(const fs::path& repo_root, const std::string& rel_path, const std::string& content)
{
    write_file(repo_root / rel_path, content);
    Blob b(content);
    ObjectDatabase db(repo_root / ".minigit" / "objects");
    db.write(b.id(), b.serialized());
    Index index(repo_root / ".minigit" / "index");
    index.add(rel_path, b.id());
    index.write();
    return b.id();
}

std::string commit_staged(const fs::path& repo_root, const std::string& message)
{
    Index index(repo_root / ".minigit" / "index");
    std::vector<TreeEntry> entries;
    for (const auto& [p, id] : index.entries())
    {
        entries.push_back({"100644", p, id});
    }

    Tree tree(std::move(entries));
    ObjectDatabase db(repo_root / ".minigit" / "objects");
    db.write(tree.id(), tree.serialized());

    Commit c(tree.id(), {}, "Test User <test@minigit>", message);
    db.write(c.id(), c.serialized());

    write_file(repo_root / ".minigit" / "refs" / "heads" / "main", c.id() + "\n");
    write_file(repo_root / ".minigit" / "HEAD", "ref: refs/heads/main\n");
    return c.id();
}

} // namespace

// ===========================================================================
// ls-files tests
// ===========================================================================

TEST_CASE(LsFiles, DefaultListsCachedFiles)
{
    const fs::path repo = create_test_repo("cached");
    stage_file(repo, "alpha.txt", "content alpha");
    stage_file(repo, "sub/beta.txt", "content beta");

    LsFilesOptions opt;
    LsFilesResult res = perform_ls_files(repo, opt);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.entries.size(), 2u);
    ASSERT_EQ(res.entries[0], "alpha.txt");
    ASSERT_EQ(res.entries[1], "sub/beta.txt");
}

TEST_CASE(LsFiles, StageFormatOutputsModeShaStagePath)
{
    const fs::path repo = create_test_repo("stage");
    const std::string sha = stage_file(repo, "foo.txt", "bar");

    LsFilesOptions opt;
    opt.stage = true;
    LsFilesResult res = perform_ls_files(repo, opt);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.entries.size(), 1u);
    const std::string expected = "100644 " + sha + " 0\tfoo.txt\n";
    ASSERT_EQ(res.output, expected);
}

TEST_CASE(LsFiles, ModifiedAndDeletedDetection)
{
    const fs::path repo = create_test_repo("mod_del");
    stage_file(repo, "keep.txt", "original");
    stage_file(repo, "modify.txt", "original");
    stage_file(repo, "delete.txt", "original");

    // Modify one in working tree
    write_file(repo / "modify.txt", "changed content");
    // Delete one in working tree
    fs::remove(repo / "delete.txt");

    // Test modified flag
    {
        LsFilesOptions opt;
        opt.modified = true;
        LsFilesResult res = perform_ls_files(repo, opt);
        ASSERT_TRUE(res.success);
        ASSERT_EQ(res.entries.size(), 1u);
        ASSERT_EQ(res.entries[0], "modify.txt");
    }

    // Test deleted flag
    {
        LsFilesOptions opt;
        opt.deleted = true;
        LsFilesResult res = perform_ls_files(repo, opt);
        ASSERT_TRUE(res.success);
        ASSERT_EQ(res.entries.size(), 1u);
        ASSERT_EQ(res.entries[0], "delete.txt");
    }
}

TEST_CASE(LsFiles, OthersRespectsIgnore)
{
    const fs::path repo = create_test_repo("others");
    stage_file(repo, "tracked.txt", "tracked");

    write_file(repo / ".minigitignore", "*.log\nbuild/\n");
    write_file(repo / "untracked.txt", "untracked");
    write_file(repo / "debug.log", "ignored log");
    write_file(repo / "build" / "out.bin", "ignored binary");

    LsFilesOptions opt;
    opt.others = true;
    LsFilesResult res = perform_ls_files(repo, opt);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.entries.size(), 1u);
    ASSERT_EQ(res.entries[0], "untracked.txt");
}

TEST_CASE(LsFiles, PathFiltering)
{
    const fs::path repo = create_test_repo("path_filter");
    stage_file(repo, "src/main.cpp", "main");
    stage_file(repo, "src/util.cpp", "util");
    stage_file(repo, "docs/index.md", "docs");

    LsFilesOptions opt;
    opt.paths = {"src"};
    LsFilesResult res = perform_ls_files(repo, opt);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.entries.size(), 2u);
    ASSERT_EQ(res.entries[0], "src/main.cpp");
    ASSERT_EQ(res.entries[1], "src/util.cpp");
}

// ===========================================================================
// ls-tree tests
// ===========================================================================

TEST_CASE(LsTree, FromCommitHEAD)
{
    const fs::path repo = create_test_repo("tree_head");
    const std::string sha_a = stage_file(repo, "file_a.txt", "AAA");
    const std::string sha_b = stage_file(repo, "file_b.txt", "BBB");
    commit_staged(repo, "Initial commit");

    LsTreeOptions opt;
    opt.tree_ish = "HEAD";
    LsTreeResult res = perform_ls_tree(repo, opt);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.entries.size(), 2u);
    ASSERT_EQ(res.entries[0].path, "file_a.txt");
    ASSERT_EQ(res.entries[0].sha, sha_a);
    ASSERT_EQ(res.entries[1].path, "file_b.txt");
    ASSERT_EQ(res.entries[1].sha, sha_b);
}

TEST_CASE(LsTree, NameOnlyAndObjectOnly)
{
    const fs::path repo = create_test_repo("name_object_only");
    const std::string sha = stage_file(repo, "test.txt", "hello");
    commit_staged(repo, "Commit hello");

    // Test name-only
    {
        LsTreeOptions opt;
        opt.tree_ish = "HEAD";
        opt.name_only = true;
        LsTreeResult res = perform_ls_tree(repo, opt);
        ASSERT_TRUE(res.success);
        ASSERT_EQ(res.output, "test.txt\n");
    }

    // Test object-only
    {
        LsTreeOptions opt;
        opt.tree_ish = "HEAD";
        opt.object_only = true;
        LsTreeResult res = perform_ls_tree(repo, opt);
        ASSERT_TRUE(res.success);
        ASSERT_EQ(res.output, sha + "\n");
    }

    // Test mutually exclusive
    {
        LsTreeOptions opt;
        opt.tree_ish = "HEAD";
        opt.name_only = true;
        opt.object_only = true;
        LsTreeResult res = perform_ls_tree(repo, opt);
        ASSERT_FALSE(res.success);
        ASSERT_TRUE(res.error_message.find("cannot be used together") != std::string::npos);
    }
}

TEST_CASE(LsTree, RecursiveAndSubtrees)
{
    const fs::path repo = create_test_repo("subtrees");

    // Construct sub-tree manually: "subdir" tree containing "inner.txt"
    Blob b("inner content");
    ObjectDatabase db(repo / ".minigit" / "objects");
    db.write(b.id(), b.serialized());

    Tree inner_tree({TreeEntry{"100644", "inner.txt", b.id()}});
    db.write(inner_tree.id(), inner_tree.serialized());

    // Root tree containing "root.txt" and "subdir"
    Blob root_b("root content");
    db.write(root_b.id(), root_b.serialized());

    Tree root_tree({
        TreeEntry{"100644", "root.txt", root_b.id()},
        TreeEntry{"040000", "subdir", inner_tree.id()}
    });
    db.write(root_tree.id(), root_tree.serialized());

    // Non-recursive: lists "root.txt" and "subdir"
    {
        LsTreeOptions opt;
        opt.tree_ish = root_tree.id();
        LsTreeResult res = perform_ls_tree(repo, opt);
        ASSERT_TRUE(res.success);
        ASSERT_EQ(res.entries.size(), 2u);
        ASSERT_EQ(res.entries[0].path, "root.txt");
        ASSERT_EQ(res.entries[0].type, "blob");
        ASSERT_EQ(res.entries[1].path, "subdir");
        ASSERT_EQ(res.entries[1].type, "tree");
    }

    // Recursive (-r): recurses into subdir
    {
        LsTreeOptions opt;
        opt.tree_ish = root_tree.id();
        opt.recursive = true;
        LsTreeResult res = perform_ls_tree(repo, opt);
        ASSERT_TRUE(res.success);
        ASSERT_EQ(res.entries.size(), 2u);
        ASSERT_EQ(res.entries[0].path, "root.txt");
        ASSERT_EQ(res.entries[1].path, "subdir/inner.txt");
    }

    // Recursive with show-trees (-r -t): shows subdir tree AND leaf files
    {
        LsTreeOptions opt;
        opt.tree_ish = root_tree.id();
        opt.recursive = true;
        opt.show_trees = true;
        LsTreeResult res = perform_ls_tree(repo, opt);
        ASSERT_TRUE(res.success);
        ASSERT_EQ(res.entries.size(), 3u);
        ASSERT_EQ(res.entries[0].path, "root.txt");
        ASSERT_EQ(res.entries[1].path, "subdir");
        ASSERT_EQ(res.entries[1].type, "tree");
        ASSERT_EQ(res.entries[2].path, "subdir/inner.txt");
    }

    // Tree-only (-d): shows only directories
    {
        LsTreeOptions opt;
        opt.tree_ish = root_tree.id();
        opt.tree_only = true;
        LsTreeResult res = perform_ls_tree(repo, opt);
        ASSERT_TRUE(res.success);
        ASSERT_EQ(res.entries.size(), 1u);
        ASSERT_EQ(res.entries[0].path, "subdir");
        ASSERT_EQ(res.entries[0].type, "tree");
    }
}

TEST_CASE(LsTree, PathFiltering)
{
    const fs::path repo = create_test_repo("tree_filter");
    stage_file(repo, "src/app.cpp", "app");
    stage_file(repo, "include/app.h", "header");
    commit_staged(repo, "Add app");

    LsTreeOptions opt;
    opt.tree_ish = "HEAD";
    opt.paths = {"include"};
    LsTreeResult res = perform_ls_tree(repo, opt);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.entries.size(), 1u);
    ASSERT_EQ(res.entries[0].path, "include/app.h");
}
