#include "test_framework.h"
#include "repository/repository.h"

#include <filesystem>

TEST_CASE(Repository, InitAndDiscover)
{
    const auto temp_dir = std::filesystem::temp_directory_path() / "minigit_repo_test";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    Repository repo(temp_dir);
    repo.init();

    ASSERT_TRUE(std::filesystem::exists(temp_dir / ".minigit"));
    ASSERT_TRUE(std::filesystem::exists(temp_dir / ".minigit" / "HEAD"));
    ASSERT_TRUE(std::filesystem::exists(temp_dir / ".minigit" / "objects"));
    ASSERT_TRUE(std::filesystem::exists(temp_dir / ".minigit" / "refs" / "heads"));
    ASSERT_TRUE(std::filesystem::exists(temp_dir / ".minigit" / "refs" / "tags"));

    // Test discovery from sub-directory
    const auto sub_dir = temp_dir / "subdir" / "nested";
    std::filesystem::create_directories(sub_dir);

    Repository discovered = Repository::discover(sub_dir);
    ASSERT_EQ(discovered.root(), std::filesystem::weakly_canonical(temp_dir));

    std::filesystem::remove_all(temp_dir);
}
