#include "test_framework.h"
#include "staging/index.h"
#include "staging/ignore.h"
#include "staging/add.h"
#include "repository/repository.h"

#include <filesystem>
#include <fstream>

TEST_CASE(Staging, IndexRoundtrip)
{
    const auto temp_index_path = std::filesystem::temp_directory_path() / "minigit_test_index";
    std::filesystem::remove(temp_index_path);

    // Create index and add files
    {
        Index index(temp_index_path);
        ASSERT_TRUE(index.entries().empty());

        index.add("src/main.cpp", "1111111111111111111111111111111111111111111111111111111111111111");
        index.add("README.md", "2222222222222222222222222222222222222222222222222222222222222222");
        index.write();
    }

    // Reload from disk and verify
    {
        Index index(temp_index_path);
        ASSERT_EQ(index.entries().size(), 2);
        ASSERT_TRUE(index.entries().count("src/main.cpp") > 0);
        ASSERT_TRUE(index.entries().count("README.md") > 0);

        // Remove an entry
        index.remove("README.md");
        index.write();
    }

    // Verify removal
    {
        Index index(temp_index_path);
        ASSERT_EQ(index.entries().size(), 1);
        ASSERT_TRUE(index.entries().count("src/main.cpp") > 0);
        ASSERT_TRUE(index.entries().count("README.md") == 0);
    }

    std::filesystem::remove(temp_index_path);
}

TEST_CASE(Staging, IgnoreRules)
{
    const auto temp_repo = std::filesystem::temp_directory_path() / "minigit_test_ignore_repo";
    std::filesystem::create_directories(temp_repo);

    {
        std::ofstream ignore_file(temp_repo / ".minigitignore");
        ignore_file << "*.log\n";
        ignore_file << "build/\n";
        ignore_file << "temp.txt\n";
    }

    IgnoreRules rules = IgnoreRules::load(temp_repo);

    ASSERT_TRUE(rules.is_ignored("app.log"));
    ASSERT_TRUE(rules.is_ignored("debug.log"));
    ASSERT_TRUE(rules.is_ignored("temp.txt"));
    ASSERT_FALSE(rules.is_ignored("main.cpp"));
    ASSERT_FALSE(rules.is_ignored("README.md"));

    std::filesystem::remove_all(temp_repo);
}

TEST_CASE(Staging, AddDotRecursive)
{
    const auto temp_repo = std::filesystem::temp_directory_path() / "minigit_test_add_dot";
    std::filesystem::remove_all(temp_repo);
    std::filesystem::create_directories(temp_repo);

    Repository repo(temp_repo);
    repo.init();

    // Create files in root and subdirectories
    {
        std::ofstream f(temp_repo / "root.txt");
        f << "root content";
    }
    std::filesystem::create_directories(temp_repo / "subdir");
    {
        std::ofstream f(temp_repo / "subdir" / "child.txt");
        f << "child content";
    }

    const auto old_cwd = std::filesystem::current_path();
    std::filesystem::current_path(temp_repo);

    ASSERT_TRUE(add_files({"."}));

    std::filesystem::current_path(old_cwd);

    Index index(repo.git_dir() / "index");
    ASSERT_EQ(index.entries().size(), 2);
    ASSERT_TRUE(index.entries().count("root.txt") > 0);
    ASSERT_TRUE(index.entries().count("subdir/child.txt") > 0);

    std::filesystem::remove_all(temp_repo);
}
