#include "test_framework.h"
#include "storage/blob.h"
#include "storage/tree.h"
#include "storage/commit.h"
#include "storage/object_parser.h"
#include "storage/object_database.h"

#include <filesystem>

TEST_CASE(Storage, BlobCreation)
{
    Blob blob("hello world\n");
    ASSERT_EQ(blob.content(), "hello world\n");
    ASSERT_FALSE(blob.id().empty());
    // Serialized envelope should contain "blob 12\0hello world\n"
    const std::string serialized = blob.serialized();
    ASSERT_TRUE(serialized.find("blob 12") != std::string::npos);
}

TEST_CASE(Storage, TreeSerialization)
{
    std::vector<TreeEntry> entries = {
        {"100644", "README.md", "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"},
        {"100644", "main.cpp", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"}
    };

    Tree tree(entries);
    ASSERT_FALSE(tree.id().empty());
    ASSERT_EQ(tree.entries().size(), 2);
}

TEST_CASE(Storage, CommitSerialization)
{
    Commit commit("tree123", {"parent1"}, "Author <a@b.com>", "Initial commit\n");
    ASSERT_FALSE(commit.id().empty());
    const std::string serialized = commit.serialized();
    ASSERT_TRUE(serialized.find("tree tree123") != std::string::npos);
    ASSERT_TRUE(serialized.find("parent parent1") != std::string::npos);
    ASSERT_TRUE(serialized.find("Initial commit") != std::string::npos);
}

TEST_CASE(Storage, ObjectDatabaseRoundtrip)
{
    const auto temp_dir = std::filesystem::temp_directory_path() / "minigit_odb_test";
    std::filesystem::create_directories(temp_dir);

    ObjectDatabase db(temp_dir);
    Blob blob("test database content");
    db.write(blob.id(), blob.serialized());

    const std::string read_bytes = db.read(blob.id());
    ASSERT_EQ(read_bytes, blob.serialized());

    std::filesystem::remove_all(temp_dir);
}
