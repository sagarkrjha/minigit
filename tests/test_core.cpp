#include "test_framework.h"
#include "core/sha256.h"
#include "core/zlib_compress.h"
#include "core/file.h"

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
