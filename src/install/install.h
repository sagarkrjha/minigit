#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minigit::install {

enum class InstallScope {
    Auto,
    User,
    System
};

std::string_view scope_to_string(InstallScope scope);
std::optional<InstallScope> parse_scope(std::string_view str);

struct InstallOptions {
    InstallScope scope{InstallScope::Auto};
    std::filesystem::path custom_dir;
    bool add_to_path{true};
    bool force{false};
    bool uninstall{false};
};

struct InstallResult {
    bool success{false};
    std::string message;
    std::filesystem::path installed_exe;
    InstallScope effective_scope{InstallScope::User};
    bool path_modified{false};
    bool escalated{false};
};

// Privilege & Permission detection
bool is_running_as_admin();
bool can_write_to_directory(const std::filesystem::path& dir);
bool can_write_to_file(const std::filesystem::path& file);

// Path resolution
std::filesystem::path get_default_install_dir(InstallScope scope);
std::filesystem::path get_current_executable_path();

// PATH list string manipulation (pure logic, fully unit testable)
bool is_in_path_list(std::string_view path_list, const std::filesystem::path& dir, char delimiter = ';');
std::string add_to_path_list(std::string_view path_list, const std::filesystem::path& dir, char delimiter = ';');
std::string remove_from_path_list(std::string_view path_list, const std::filesystem::path& dir, char delimiter = ';');

// Environment PATH modification
bool is_directory_in_env_path(const std::filesystem::path& dir, InstallScope scope);
bool add_directory_to_env_path(const std::filesystem::path& dir, InstallScope scope);
bool remove_directory_from_env_path(const std::filesystem::path& dir, InstallScope scope);

// UAC Elevation
bool request_uac_elevation(const std::filesystem::path& exe, const std::vector<std::string>& args, int& exit_code);

// Execution
InstallResult perform_install(const InstallOptions& options, const std::filesystem::path& source_exe = {});
InstallResult perform_uninstall(const InstallOptions& options);

// CLI Command entry point
int install_command(int argc, char const *argv[]);

} // namespace minigit::install
