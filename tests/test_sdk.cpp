#include "test_framework.h"
#include "sdk/minigit_sdk.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

TEST_CASE(sdk_init_and_open)
{
    fs::path test_dir = fs::temp_directory_path() / "minigit_sdk_test_repo";
    if (fs::exists(test_dir)) {
        fs::remove_all(test_dir);
    }

    auto client = minigit::sdk::MiniGitClient::init(test_dir);
    ASSERT_TRUE(client.is_valid());
    ASSERT_TRUE(fs::exists(test_dir / ".minigit"));

    auto opened = minigit::sdk::MiniGitClient::open(test_dir);
    ASSERT_TRUE(opened.is_valid());
    ASSERT_EQ(opened.current_branch(), "main");

    fs::remove_all(test_dir);
}

TEST_CASE(sdk_stage_and_commit)
{
    fs::path test_dir = fs::temp_directory_path() / "minigit_sdk_test_commit";
    if (fs::exists(test_dir)) {
        fs::remove_all(test_dir);
    }

    auto client = minigit::sdk::MiniGitClient::init(test_dir);
    ASSERT_TRUE(client.is_valid());

    // Create a sample file
    std::ofstream ofs(test_dir / "sample.txt");
    ofs << "MiniGit SDK integration test\n";
    ofs.close();

    // Stage file
    bool added = client.add({"sample.txt"});
    ASSERT_TRUE(added);

    // Commit
    std::string cid = client.commit("Initial SDK commit", "SDK Tester <tester@minigit>");
    ASSERT_FALSE(cid.empty());
    ASSERT_EQ(cid.size(), 64);

    // Check status
    auto status = client.status();
    ASSERT_TRUE(status.is_clean);
    ASSERT_EQ(status.head_commit_id, cid);

    // Verify log
    auto logs = client.log(5);
    ASSERT_EQ(logs.size(), 1);
    ASSERT_EQ(logs[0].commit_id, cid);
    ASSERT_EQ(logs[0].message, "Initial SDK commit");

    // Verify commit inspection
    auto commit_info = client.get_commit(cid);
    ASSERT_TRUE(commit_info.has_value());
    ASSERT_EQ(commit_info->author, "SDK Tester <tester@minigit>");

    fs::remove_all(test_dir);
}

TEST_CASE(sdk_branching)
{
    fs::path test_dir = fs::temp_directory_path() / "minigit_sdk_test_branch";
    if (fs::exists(test_dir)) {
        fs::remove_all(test_dir);
    }

    auto client = minigit::sdk::MiniGitClient::init(test_dir);
    std::ofstream ofs(test_dir / "foo.txt");
    ofs << "foo\n";
    ofs.close();

    client.add_all();
    std::string cid = client.commit("Commit on main");
    ASSERT_FALSE(cid.empty());

    bool branched = client.create_branch("develop");
    ASSERT_TRUE(branched);

    auto branches = client.list_branches();
    ASSERT_EQ(branches.size(), 2);

    bool switched = client.switch_branch("develop");
    ASSERT_TRUE(switched);
    ASSERT_EQ(client.current_branch(), "develop");

    fs::remove_all(test_dir);
}

TEST_CASE(sdk_cloud_sync_and_restore)
{
    fs::path repo_dir = fs::temp_directory_path() / "minigit_sdk_source_repo";
    fs::path backup_dir = fs::temp_directory_path() / "minigit_sdk_cloud_store";
    fs::path restored_repo_dir = fs::temp_directory_path() / "minigit_sdk_restored_repo";

    fs::remove_all(repo_dir);
    fs::remove_all(backup_dir);
    fs::remove_all(restored_repo_dir);

    auto client = minigit::sdk::MiniGitClient::init(repo_dir);
    std::ofstream ofs(repo_dir / "cloud_test.txt");
    ofs << "Cloud storage contents\n";
    ofs.close();

    client.add_all();
    std::string cid = client.commit("Cloud sync test commit");
    ASSERT_FALSE(cid.empty());

    // Sync to local cloud target (emulating S3 bucket location)
    bool synced = client.sync_to_cloud(backup_dir.string(), "backups/test-repo");
    ASSERT_TRUE(synced);
    ASSERT_TRUE(fs::exists(backup_dir / "backups" / "test-repo" / "HEAD"));
    ASSERT_TRUE(fs::exists(backup_dir / "backups" / "test-repo" / "objects"));

    // Restore to a fresh repo
    auto restored_client = minigit::sdk::MiniGitClient::init(restored_repo_dir);
    bool restored = restored_client.restore_from_cloud(backup_dir.string(), "backups/test-repo");
    ASSERT_TRUE(restored);

    auto restored_logs = restored_client.log(5);
    ASSERT_EQ(restored_logs.size(), 1);
    ASSERT_EQ(restored_logs[0].commit_id, cid);

    fs::remove_all(repo_dir);
    fs::remove_all(backup_dir);
    fs::remove_all(restored_repo_dir);
}

TEST_CASE(sdk_c_bindings)
{
    fs::path test_dir = fs::temp_directory_path() / "minigit_c_sdk_test";
    if (fs::exists(test_dir)) {
        fs::remove_all(test_dir);
    }

    minigit_client_handle_t handle = minigit_sdk_init(test_dir.string().c_str());
    ASSERT_TRUE(handle != nullptr);

    std::ofstream ofs(test_dir / "c_api.txt");
    ofs << "C API test\n";
    ofs.close();

    ASSERT_EQ(minigit_sdk_add_all(handle), 1);

    char* cid = minigit_sdk_commit(handle, "Commit via C API", "C Programmer <c@minigit>");
    ASSERT_TRUE(cid != nullptr);
    ASSERT_TRUE(std::strlen(cid) == 64);
    minigit_sdk_string_free(cid);

    char* branch = minigit_sdk_get_current_branch(handle);
    ASSERT_TRUE(branch != nullptr);
    ASSERT_EQ(std::string(branch), "main");
    minigit_sdk_string_free(branch);

    ASSERT_EQ(minigit_sdk_create_branch(handle, "c_feature"), 1);
    ASSERT_EQ(minigit_sdk_switch_branch(handle, "c_feature"), 1);

    minigit_sdk_free(handle);
    fs::remove_all(test_dir);
}
