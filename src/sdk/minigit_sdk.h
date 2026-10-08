
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>

/**
 * @file minigit_sdk.h
 * @brief Public C++ and C SDK for programmatic integration with MiniGit repositories.
 */

#ifdef _WIN32
  #if defined(MINIGIT_SDK_EXPORTS)
    #define MINIGIT_SDK_API __declspec(dllexport)
  #elif defined(MINIGIT_SDK_STATIC)
    #define MINIGIT_SDK_API
  #else
    #define MINIGIT_SDK_API
  #endif
#else
  #define MINIGIT_SDK_API __attribute__((visibility("default")))
#endif

namespace minigit::sdk {

struct CommitInfo {
    std::string commit_id;
    std::string tree_id;
    std::vector<std::string> parent_ids;
    std::string author;
    std::string message;
    std::string timestamp;
};

struct StatusEntry {
    std::string path;
    std::string staged_status;   // e.g., "added", "modified", "deleted"
    std::string unstaged_status; // e.g., "modified", "deleted", "untracked"
};

struct RepositoryStatus {
    std::string current_branch;
    std::string head_commit_id;
    std::vector<StatusEntry> entries;
    bool is_clean{true};
};

/**
 * @brief Client for programmatic MiniGit operations on local repositories.
 */
class MINIGIT_SDK_API MiniGitClient {
public:
    MiniGitClient();
    explicit MiniGitClient(const std::filesystem::path& repo_root);
    ~MiniGitClient();

    // Repository lifecycle
    static MiniGitClient init(const std::filesystem::path& path);
    static MiniGitClient open(const std::filesystem::path& path);

    bool is_valid() const;
    std::filesystem::path root_path() const;
    std::filesystem::path git_dir_path() const;

    // Daily development workflow
    bool add(const std::vector<std::string>& filepaths);
    bool add_all();
    std::string commit(const std::string& message, const std::string& author = "");
    RepositoryStatus status() const;

    // Branching & Head
    std::vector<std::string> list_branches() const;
    std::string current_branch() const;
    bool create_branch(const std::string& branch_name);
    bool switch_branch(const std::string& branch_name);

    // History & Inspection
    std::vector<CommitInfo> log(size_t max_count = 50) const;
    std::optional<CommitInfo> get_commit(const std::string& commit_id) const;
    std::string get_object_content(const std::string& object_id) const;

    // AWS Cloud Sync / Backup (S3 / Local Object Storage Bridge)
    bool sync_to_cloud(const std::string& bucket_or_local_target, const std::string& prefix = "");
    bool restore_from_cloud(const std::string& bucket_or_local_source, const std::string& prefix = "");

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace minigit::sdk

/* ========================================================================= */
/* C-compatible API bindings for FFI, Python ctypes, or cross-language tools */
/* ========================================================================= */

#ifdef __cplusplus
extern "C" {
#endif

typedef void* minigit_client_handle_t;

MINIGIT_SDK_API minigit_client_handle_t minigit_sdk_init(const char* path);
MINIGIT_SDK_API minigit_client_handle_t minigit_sdk_open(const char* path);
MINIGIT_SDK_API void minigit_sdk_free(minigit_client_handle_t handle);

MINIGIT_SDK_API int minigit_sdk_add(minigit_client_handle_t handle, const char* filepath);
MINIGIT_SDK_API int minigit_sdk_add_all(minigit_client_handle_t handle);
MINIGIT_SDK_API char* minigit_sdk_commit(minigit_client_handle_t handle, const char* message, const char* author);
MINIGIT_SDK_API char* minigit_sdk_get_current_branch(minigit_client_handle_t handle);
MINIGIT_SDK_API int minigit_sdk_create_branch(minigit_client_handle_t handle, const char* branch_name);
MINIGIT_SDK_API int minigit_sdk_switch_branch(minigit_client_handle_t handle, const char* branch_name);

MINIGIT_SDK_API int minigit_sdk_sync_to_cloud(minigit_client_handle_t handle, const char* target, const char* prefix);
MINIGIT_SDK_API int minigit_sdk_restore_from_cloud(minigit_client_handle_t handle, const char* source, const char* prefix);

MINIGIT_SDK_API void minigit_sdk_string_free(char* str);

#ifdef __cplusplus
}
#endif
