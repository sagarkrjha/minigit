#include "test_framework.h"
#include "core/sha256.h"
#include "core/zlib_compress.h"
#include "core/file.h"
#include "core/path_safety.h"

#include <filesystem>
#include <fstream>

TEST_CASE(Core, Sha256Empty)
{
    const std::string empty_hash = sha256("");
    ASSERT_EQ(empty_hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE(Core, Sha256Hello)
{
    const std::string hello_hash = sha256("hello");
    ASSERT_EQ(hello_hash, "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
}

TEST_CASE(Core, ZlibRoundtrip)
{
    const std::string input = "MiniGit core compression test string with some repetitions: abc abc abc 123 123 123";

    std::string compressed = zlib_compress(input);
    ASSERT_TRUE(!compressed.empty());

    std::string decompressed = zlib_decompress(compressed);
    ASSERT_EQ(decompressed, input);
}

TEST_CASE(Core, FileRead)
{
    const auto temp_path = std::filesystem::temp_directory_path() / "minigit_core_test.txt";
    const std::string content = "Testing file I/O operations\nLine 2";

    {
        std::ofstream out(temp_path, std::ios::binary);
        out << content;
    }

    std::string read_data = read_file(temp_path);
    ASSERT_EQ(read_data, content);

    std::filesystem::remove(temp_path);
}

TEST_CASE(Core, PathSafetySafePaths)
{
    ASSERT_TRUE(minigit::core::is_safe_repo_relpath("README.md"));
    ASSERT_TRUE(minigit::core::is_safe_repo_relpath("src/core/main.cpp"));
    ASSERT_TRUE(minigit::core::is_safe_repo_relpath("a/b/c/d/e.txt"));
}

TEST_CASE(Core, PathSafetyTraversalRejected)
{
    // Traversal attempts
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath("../foo"));
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath("../../etc/passwd"));
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath("foo/../../bar"));
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath("a/b/../../../c"));
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath("/root/file.txt"));
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath("C:/Windows/system32"));
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath(""));
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath("."));

    // Internal metadata directory attempts
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath(".minigit/config"));
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath(".git/HEAD"));
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath(".minigit"));
    ASSERT_FALSE(minigit::core::is_safe_repo_relpath(".git"));
}

TEST_CASE(Core, PathSafetyResolution)
{
    const auto root = std::filesystem::temp_directory_path() / "minigit_safe_root";
    std::filesystem::create_directories(root);

    const auto safe_p = minigit::core::resolve_safe_repo_path(root, "docs/manual.pdf");
    ASSERT_EQ(safe_p, (root / "docs" / "manual.pdf").lexically_normal());

    bool caught = false;
    try
    {
        minigit::core::resolve_safe_repo_path(root, "../malicious.sh");
    }
    catch (const std::exception &)
    {
        caught = true;
    }
    ASSERT_TRUE(caught);

    std::filesystem::remove_all(root);
}

