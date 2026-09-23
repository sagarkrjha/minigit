#include "install.h"
#include "core/version.h"
#include "core/logger.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#include <sys/stat.h>
#elif defined(__linux__)
#include <unistd.h>
#include <sys/stat.h>
#include <climits>
#endif

namespace minigit::install {

std::string_view scope_to_string(InstallScope scope) {
    switch (scope) {
        case InstallScope::User:   return "User";
        case InstallScope::System: return "System";
        case InstallScope::Auto:   return "Auto";
    }
    return "Auto";
}

std::optional<InstallScope> parse_scope(std::string_view str) {
    if (str == "user" || str == "User") {
        return InstallScope::User;
    }
    if (str == "system" || str == "System") {
        return InstallScope::System;
    }
    if (str == "auto" || str == "Auto") {
        return InstallScope::Auto;
    }
    return std::nullopt;
}

bool is_running_as_admin() {
    const char* env_override = std::getenv("MINIGIT_ADMIN_OVERRIDE");
    if (env_override) {
        std::string_view s(env_override);
        if (s == "1" || s == "true" || s == "yes") return true;
        if (s == "0" || s == "false" || s == "no") return false;
    }

#if defined(_WIN32)
    BOOL is_admin = FALSE;
    PSID admin_group = NULL;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                0, 0, 0, 0, 0, 0, &admin_group)) {
        CheckTokenMembership(NULL, admin_group, &is_admin);
        FreeSid(admin_group);
    }
    return is_admin != FALSE;
#elif defined(__unix__) || defined(__APPLE__)
    return geteuid() == 0;
#else
    return false;
#endif
}

bool can_write_to_directory(const std::filesystem::path& dir) {
    const char* env_override = std::getenv("MINIGIT_WRITE_PERM_OVERRIDE");
    if (env_override) {
        std::string_view s(env_override);
        if (s == "1" || s == "true" || s == "yes") return true;
        if (s == "0" || s == "false" || s == "no") return false;
    }

    std::filesystem::path probe_dir = dir;
    std::error_code ec;

    while (!probe_dir.empty() && !std::filesystem::exists(probe_dir, ec)) {
        auto parent = probe_dir.parent_path();
        if (parent == probe_dir) break;
        probe_dir = parent;
    }

    if (probe_dir.empty() || !std::filesystem::exists(probe_dir, ec)) {
        return false;
    }

    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto probe_file = probe_dir / (".minigit_probe_" + std::to_string(now) + ".tmp");
    {
        std::ofstream out(probe_file, std::ios::out | std::ios::trunc);
        if (!out) return false;
        out << "probe";
    }
    std::filesystem::remove(probe_file, ec);
    return true;
}

bool can_write_to_file(const std::filesystem::path& file) {
    const char* env_override = std::getenv("MINIGIT_WRITE_PERM_OVERRIDE");
    if (env_override) {
        std::string_view s(env_override);
        if (s == "1" || s == "true" || s == "yes") return true;
        if (s == "0" || s == "false" || s == "no") return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) {
        return can_write_to_directory(file.parent_path());
    }

#if defined(_WIN32)
    HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        return true;
    }
    return false;
#else
    return access(file.c_str(), W_OK) == 0;
#endif
}

std::filesystem::path get_default_install_dir(InstallScope scope) {
    const char* env_override = std::getenv("MINIGIT_INSTALL_DIR_OVERRIDE");
    if (env_override && *env_override) {
        return std::filesystem::path(env_override);
    }

    if (scope == InstallScope::System) {
#if defined(_WIN32)
        const char* pf = std::getenv("ProgramFiles");
        std::filesystem::path base = (pf && *pf) ? std::filesystem::path(pf) : std::filesystem::path("C:\\Program Files");
        return base / "minigit" / "bin";
#else
        return std::filesystem::path("/usr/local/bin");
#endif
    } else { // User or Auto default
#if defined(_WIN32)
        const char* localappdata = std::getenv("LOCALAPPDATA");
        if (localappdata && *localappdata) {
            return std::filesystem::path(localappdata) / "Programs" / "minigit" / "bin";
        }
        const char* userprofile = std::getenv("USERPROFILE");
        if (userprofile && *userprofile) {
            return std::filesystem::path(userprofile) / ".minigit" / "bin";
        }
        return std::filesystem::path("C:\\minigit\\bin");
#else
        const char* home = std::getenv("HOME");
        if (home && *home) {
            return std::filesystem::path(home) / ".local" / "bin";
        }
        return std::filesystem::path("/usr/local/bin");
#endif
    }
}

std::filesystem::path get_current_executable_path() {
    const char* env_path = std::getenv("MINIGIT_EXEC_PATH_OVERRIDE");
    if (env_path && *env_path) {
        return std::filesystem::path(env_path);
    }

#if defined(_WIN32)
    std::wstring path_buf(MAX_PATH, L'\0');
    DWORD len = GetModuleFileNameW(NULL, path_buf.data(), static_cast<DWORD>(path_buf.size()));
    while (len == path_buf.size()) {
        path_buf.resize(path_buf.size() * 2);
        len = GetModuleFileNameW(NULL, path_buf.data(), static_cast<DWORD>(path_buf.size()));
    }
    if (len > 0) {
        path_buf.resize(len);
        return std::filesystem::path(path_buf);
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size > 0) {
        std::vector<char> buf(size);
        if (_NSGetExecutablePath(buf.data(), &size) == 0) {
            std::error_code ec;
            auto can = std::filesystem::canonical(buf.data(), ec);
            if (!ec) return can;
            return std::filesystem::path(buf.data());
        }
    }
#elif defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::error_code ec;
        auto can = std::filesystem::canonical(buf, ec);
        if (!ec) return can;
        return std::filesystem::path(buf);
    }
#endif

    return {};
}

namespace {

bool is_case_insensitive_path(char delimiter) {
#if defined(_WIN32)
    (void)delimiter;
    return true;
#else
    return delimiter == ';';
#endif
}

std::string normalize_path_for_compare(const std::filesystem::path& p, bool case_insensitive) {
    std::string s = p.lexically_normal().string();
    // Normalize slashes to forward slashes for comparison
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') {
        s.pop_back();
    }
    if (case_insensitive) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }
    return s;
}

std::vector<std::string> split_path_list(std::string_view path_list, char delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start < path_list.size()) {
        size_t end = path_list.find(delimiter, start);
        if (end == std::string_view::npos) {
            end = path_list.size();
        }
        std::string_view part = path_list.substr(start, end - start);
        // Trim leading and trailing whitespace
        while (!part.empty() && (part.front() == ' ' || part.front() == '\t')) {
            part.remove_prefix(1);
        }
        while (!part.empty() && (part.back() == ' ' || part.back() == '\t')) {
            part.remove_suffix(1);
        }
        if (!part.empty()) {
            result.emplace_back(part);
        }
        start = end + 1;
    }
    return result;
}

#if defined(_WIN32)
std::wstring utf8_to_wstring(std::string_view str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size);
    return result;
}

std::string wstring_to_utf8(std::wstring_view wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size, NULL, NULL);
    return result;
}
#endif

} // namespace

bool is_in_path_list(std::string_view path_list, const std::filesystem::path& dir, char delimiter) {
    bool ci = is_case_insensitive_path(delimiter);
    std::string target_norm = normalize_path_for_compare(dir, ci);
    if (target_norm.empty()) return false;

    auto entries = split_path_list(path_list, delimiter);
    for (const auto& entry : entries) {
        if (normalize_path_for_compare(entry, ci) == target_norm) {
            return true;
        }
    }
    return false;
}

std::string add_to_path_list(std::string_view path_list, const std::filesystem::path& dir, char delimiter) {
    if (is_in_path_list(path_list, dir, delimiter)) {
        return std::string(path_list);
    }
    std::string dir_str = dir.string();
    if (path_list.empty()) {
        return dir_str;
    }
    std::string result(path_list);
    if (result.back() != delimiter) {
        result += delimiter;
    }
    result += dir_str;
    return result;
}

std::string remove_from_path_list(std::string_view path_list, const std::filesystem::path& dir, char delimiter) {
    bool ci = is_case_insensitive_path(delimiter);
    std::string target_norm = normalize_path_for_compare(dir, ci);
    if (target_norm.empty()) return std::string(path_list);

    auto entries = split_path_list(path_list, delimiter);
    std::ostringstream oss;
    bool first = true;
    for (const auto& entry : entries) {
        if (normalize_path_for_compare(entry, ci) == target_norm) {
            continue;
        }
        if (!first) {
            oss << delimiter;
        }
        oss << entry;
        first = false;
    }
    return oss.str();
}

bool is_directory_in_env_path(const std::filesystem::path& dir, InstallScope scope) {
    const char* mock_path = std::getenv("MINIGIT_MOCK_ENV_PATH");
    if (mock_path) {
        return is_in_path_list(mock_path, dir);
    }

#if defined(_WIN32)
    HKEY hRoot = (scope == InstallScope::System) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* subkey = (scope == InstallScope::System)
        ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"
        : L"Environment";

    HKEY hKey = NULL;
    if (RegOpenKeyExW(hRoot, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD size = 0;
    LONG res = RegQueryValueExW(hKey, L"Path", NULL, &type, NULL, &size);
    if (res != ERROR_SUCCESS || size == 0) {
        RegCloseKey(hKey);
        return false;
    }

    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, 0);
    res = RegQueryValueExW(hKey, L"Path", NULL, &type, reinterpret_cast<LPBYTE>(buf.data()), &size);
    RegCloseKey(hKey);

    if (res != ERROR_SUCCESS) {
        return false;
    }

    std::string path_str = wstring_to_utf8(buf.data());
    return is_in_path_list(path_str, dir, ';');
#else
    (void)scope;
    const char* env_path = std::getenv("PATH");
    if (!env_path) return false;
    return is_in_path_list(env_path, dir, ':');
#endif
}

bool add_directory_to_env_path(const std::filesystem::path& dir, InstallScope scope) {
    const char* mock_path = std::getenv("MINIGIT_MOCK_ENV_PATH");
    if (mock_path) {
        std::string updated = add_to_path_list(mock_path, dir);
#if defined(_WIN32)
        _putenv_s("MINIGIT_MOCK_ENV_PATH", updated.c_str());
#else
        setenv("MINIGIT_MOCK_ENV_PATH", updated.c_str(), 1);
#endif
        return true;
    }

#if defined(_WIN32)
    HKEY hRoot = (scope == InstallScope::System) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* subkey = (scope == InstallScope::System)
        ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"
        : L"Environment";

    HKEY hKey = NULL;
    if (RegOpenKeyExW(hRoot, subkey, 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        LOG_DEBUG("install", "failed to open registry key for environment path");
        return false;
    }

    DWORD type = REG_EXPAND_SZ;
    DWORD size = 0;
    std::string existing_path;

    if (RegQueryValueExW(hKey, L"Path", NULL, &type, NULL, &size) == ERROR_SUCCESS && size > 0) {
        std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, 0);
        if (RegQueryValueExW(hKey, L"Path", NULL, &type, reinterpret_cast<LPBYTE>(buf.data()), &size) == ERROR_SUCCESS) {
            existing_path = wstring_to_utf8(buf.data());
        }
    }

    std::string new_path = add_to_path_list(existing_path, dir, ';');
    if (new_path == existing_path) {
        RegCloseKey(hKey);
        return true; // Already present
    }

    std::wstring w_new = utf8_to_wstring(new_path);
    LONG set_res = RegSetValueExW(hKey, L"Path", 0, type,
                                  reinterpret_cast<const BYTE*>(w_new.c_str()),
                                  static_cast<DWORD>((w_new.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);

    if (set_res != ERROR_SUCCESS) {
        LOG_DEBUG("install", "failed to update registry Path: error " << set_res);
        return false;
    }

    DWORD_PTR dwResult = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment",
                        SMTO_ABORTIFHUNG, 5000, &dwResult);
    return true;
#else
    (void)dir; (void)scope;
    // On POSIX, modifying system/user PATH typically requires modifying shell rc files
    return true;
#endif
}

bool remove_directory_from_env_path(const std::filesystem::path& dir, InstallScope scope) {
    const char* mock_path = std::getenv("MINIGIT_MOCK_ENV_PATH");
    if (mock_path) {
        std::string updated = remove_from_path_list(mock_path, dir);
#if defined(_WIN32)
        _putenv_s("MINIGIT_MOCK_ENV_PATH", updated.c_str());
#else
        setenv("MINIGIT_MOCK_ENV_PATH", updated.c_str(), 1);
#endif
        return true;
    }

#if defined(_WIN32)
    HKEY hRoot = (scope == InstallScope::System) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* subkey = (scope == InstallScope::System)
        ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"
        : L"Environment";

    HKEY hKey = NULL;
    if (RegOpenKeyExW(hRoot, subkey, 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = REG_EXPAND_SZ;
    DWORD size = 0;
    std::string existing_path;

    if (RegQueryValueExW(hKey, L"Path", NULL, &type, NULL, &size) == ERROR_SUCCESS && size > 0) {
        std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, 0);
        if (RegQueryValueExW(hKey, L"Path", NULL, &type, reinterpret_cast<LPBYTE>(buf.data()), &size) == ERROR_SUCCESS) {
            existing_path = wstring_to_utf8(buf.data());
        }
    }

    std::string new_path = remove_from_path_list(existing_path, dir, ';');
    if (new_path == existing_path) {
        RegCloseKey(hKey);
        return true;
    }

    std::wstring w_new = utf8_to_wstring(new_path);
    LONG set_res = RegSetValueExW(hKey, L"Path", 0, type,
                                  reinterpret_cast<const BYTE*>(w_new.c_str()),
                                  static_cast<DWORD>((w_new.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);

    if (set_res != ERROR_SUCCESS) {
        return false;
    }

    DWORD_PTR dwResult = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment",
                        SMTO_ABORTIFHUNG, 5000, &dwResult);
    return true;
#else
    (void)dir; (void)scope;
    return true;
#endif
}

bool request_uac_elevation(const std::filesystem::path& exe, const std::vector<std::string>& args, int& exit_code) {
    const char* skip_elev = std::getenv("MINIGIT_SKIP_ELEVATION");
    if (skip_elev && (*skip_elev == '1' || std::strcmp(skip_elev, "true") == 0)) {
        LOG_DEBUG("install", "skipping UAC elevation due to MINIGIT_SKIP_ELEVATION");
        return false;
    }

#if defined(_WIN32)
    std::wstring w_exe = exe.wstring();
    std::wstring w_args;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) w_args += L" ";
        bool has_space = args[i].find(' ') != std::string::npos;
        if (has_space) w_args += L"\"";
        w_args += utf8_to_wstring(args[i]);
        if (has_space) w_args += L"\"";
    }

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = w_exe.c_str();
    sei.lpParameters = w_args.c_str();
    sei.nShow = SW_SHOWNORMAL;

    LOG_DEBUG("install", "launching elevated process: " << exe.string() << " with parameters: " << wstring_to_utf8(w_args));
    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, INFINITE);
            DWORD code = 0;
            GetExitCodeProcess(sei.hProcess, &code);
            CloseHandle(sei.hProcess);
            exit_code = static_cast<int>(code);
            return true;
        }
    }

    DWORD err = GetLastError();
    LOG_DEBUG("install", "ShellExecuteExW runas failed or was cancelled by user: error " << err);
    return false;
#else
    (void)exe; (void)args; (void)exit_code;
    return false;
#endif
}

InstallResult perform_install(const InstallOptions& options, const std::filesystem::path& source_exe) {
    InstallResult result;

    InstallScope effective_scope = options.scope;
    if (effective_scope == InstallScope::Auto) {
        effective_scope = is_running_as_admin() ? InstallScope::System : InstallScope::User;
    }
    result.effective_scope = effective_scope;

    std::filesystem::path target_dir = options.custom_dir.empty()
        ? get_default_install_dir(effective_scope)
        : options.custom_dir;

    std::filesystem::path src = source_exe.empty() ? get_current_executable_path() : source_exe;
    if (src.empty()) {
        result.message = "Unable to determine current minigit executable path";
        return result;
    }

    std::error_code ec;
    if (!std::filesystem::exists(src, ec)) {
        result.message = "Source executable does not exist at " + src.string();
        return result;
    }

    // Permission check for target installation directory
    if (!can_write_to_directory(target_dir)) {
        if (effective_scope == InstallScope::System || !options.custom_dir.empty()) {
#if defined(_WIN32)
            // Lacking permissions to system directory on Windows -> attempt UAC elevation
            std::vector<std::string> args = {"install"};
            if (options.scope == InstallScope::System) args.push_back("--system");
            if (!options.custom_dir.empty()) {
                args.push_back("--dir");
                args.push_back(options.custom_dir.string());
            }
            if (!options.add_to_path) args.push_back("--no-path");
            if (options.force) args.push_back("--force");

            int exit_code = 0;
            if (request_uac_elevation(src, args, exit_code)) {
                result.success = (exit_code == 0);
                result.escalated = true;
                result.installed_exe = target_dir / src.filename();
                if (result.success) {
                    result.message = "Successfully installed minigit via elevated Administrator privileges";
                } else {
                    result.message = "Elevated installation failed with exit code " + std::to_string(exit_code);
                }
                return result;
            }
#endif
            result.message = "Administrator privileges required to install to " + target_dir.string() +
                             ". Permission denied or elevation prompt declined.";
            return result;
        } else {
            result.message = "Permission denied: unable to write to " + target_dir.string();
            return result;
        }
    }

    // Create target directory if it doesn't exist
    if (!std::filesystem::exists(target_dir, ec)) {
        std::filesystem::create_directories(target_dir, ec);
        if (ec) {
            result.message = "Failed to create target directory: " + ec.message();
            return result;
        }
    }

    std::filesystem::path dest_filename = src.filename();
#if defined(_WIN32)
    if (dest_filename.extension() != ".exe") {
        dest_filename += ".exe";
    }
#endif
    std::filesystem::path target_exe = target_dir / dest_filename;
    result.installed_exe = target_exe;

    // Check if source and target are the same file
    if (std::filesystem::exists(target_exe, ec)) {
        auto can_src = std::filesystem::weakly_canonical(src, ec);
        auto can_tgt = std::filesystem::weakly_canonical(target_exe, ec);
        if (can_src == can_tgt) {
            result.success = true;
            result.message = "minigit is already installed at " + target_exe.string();
            if (options.add_to_path) {
                result.path_modified = add_directory_to_env_path(target_dir, effective_scope);
            }
            return result;
        }
    }

    // Safely copy or replace executable
    if (std::filesystem::exists(target_exe, ec)) {
#if defined(_WIN32)
        std::filesystem::path old_file = target_exe;
        old_file += ".old";
        if (std::filesystem::exists(old_file, ec)) {
            std::filesystem::remove(old_file, ec);
        }
        std::filesystem::rename(target_exe, old_file, ec);
        if (ec) {
            result.message = "Failed to stage existing binary for replacement: " + ec.message();
            return result;
        }
        std::filesystem::copy_file(src, target_exe, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            std::filesystem::rename(old_file, target_exe, ec);
            result.message = "Failed to copy binary to destination: " + ec.message();
            return result;
        }
        std::filesystem::remove(old_file, ec);
#else
        std::filesystem::copy_file(src, target_exe, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            result.message = "Failed to copy binary to destination: " + ec.message();
            return result;
        }
#endif
    } else {
        std::filesystem::copy_file(src, target_exe, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            result.message = "Failed to copy binary to destination: " + ec.message();
            return result;
        }
    }

#if !defined(_WIN32)
    // Ensure executable permissions on POSIX
    std::filesystem::permissions(target_exe,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec |
        std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
        std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
        std::filesystem::perm_options::replace, ec);
#endif

    // Environment PATH registration
    if (options.add_to_path) {
        result.path_modified = add_directory_to_env_path(target_dir, effective_scope);
    }

    result.success = true;
    result.message = "Successfully installed minigit to " + target_exe.string();
    return result;
}

InstallResult perform_uninstall(const InstallOptions& options) {
    InstallResult result;

    InstallScope effective_scope = options.scope;
    if (effective_scope == InstallScope::Auto) {
        effective_scope = is_running_as_admin() ? InstallScope::System : InstallScope::User;
    }
    result.effective_scope = effective_scope;

    std::filesystem::path target_dir = options.custom_dir.empty()
        ? get_default_install_dir(effective_scope)
        : options.custom_dir;

    std::string exe_name = "minigit";
#if defined(_WIN32)
    exe_name += ".exe";
#endif
    std::filesystem::path target_exe = target_dir / exe_name;
    result.installed_exe = target_exe;

    if (!can_write_to_directory(target_dir) || (std::filesystem::exists(target_exe) && !can_write_to_file(target_exe))) {
#if defined(_WIN32)
        std::filesystem::path current_exe = get_current_executable_path();
        std::vector<std::string> args = {"install", "--uninstall"};
        if (options.scope == InstallScope::System) args.push_back("--system");
        if (!options.custom_dir.empty()) {
            args.push_back("--dir");
            args.push_back(options.custom_dir.string());
        }
        int exit_code = 0;
        if (request_uac_elevation(current_exe, args, exit_code)) {
            result.success = (exit_code == 0);
            result.escalated = true;
            result.message = result.success ? "Successfully uninstalled minigit via elevated privileges" : "Elevated uninstall failed";
            return result;
        }
#endif
        result.message = "Administrator privileges required to uninstall from " + target_dir.string();
        return result;
    }

    std::error_code ec;
    if (std::filesystem::exists(target_exe, ec)) {
        std::filesystem::remove(target_exe, ec);
        if (ec) {
            result.message = "Failed to remove binary at " + target_exe.string() + ": " + ec.message();
            return result;
        }
    }

    std::filesystem::path alt_exe = target_dir / (exe_name == "minigit" ? "minigit.exe" : "minigit");
    if (std::filesystem::exists(alt_exe, ec)) {
        std::filesystem::remove(alt_exe, ec);
    }

    if (options.add_to_path) {
        result.path_modified = remove_directory_from_env_path(target_dir, effective_scope);
    }

    result.success = true;
    result.message = "Successfully uninstalled minigit from " + target_dir.string();
    return result;
}

namespace {

void print_install_usage() {
    std::cout << "usage: minigit install [--system | --user] [--dir <path>] [--no-path] [-f | --force] [--uninstall]\n\n"
              << "Install or uninstall minigit with permission-aware setup and PATH configuration.\n\n"
              << "Options:\n"
              << "  --system         Install system-wide for all users (requires Administrator privileges)\n"
              << "  --user           Install for the current user only (no elevation required)\n"
              << "  --dir <path>     Specify custom destination directory\n"
              << "  --no-path        Do not modify the environment PATH\n"
              << "  -f, --force      Reinstall/overwrite existing installation\n"
              << "  --uninstall      Uninstall minigit and remove from PATH\n"
              << "  -h, --help       Show this help message\n";
}

} // namespace

int install_command(int argc, char const *argv[]) {
    InstallOptions options;

    for (int i = 2; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--system") {
            options.scope = InstallScope::System;
        } else if (arg == "--user") {
            options.scope = InstallScope::User;
        } else if (arg == "--no-path") {
            options.add_to_path = false;
        } else if (arg == "-f" || arg == "--force") {
            options.force = true;
        } else if (arg == "--uninstall") {
            options.uninstall = true;
        } else if (arg == "-h" || arg == "--help") {
            print_install_usage();
            return 0;
        } else if (arg == "--dir") {
            if (i + 1 < argc) {
                options.custom_dir = argv[++i];
            } else {
                std::cerr << "error: --dir requires a directory path\n";
                return 1;
            }
        } else {
            std::cerr << "error: unknown argument '" << arg << "'\n";
            print_install_usage();
            return 1;
        }
    }

    if (options.uninstall) {
        std::cout << "Uninstalling minigit...\n";
        auto res = perform_uninstall(options);
        if (res.escalated) {
            return res.success ? 0 : 1;
        }
        if (!res.success) {
            std::cerr << "error: " << res.message << "\n";
            return 1;
        }
        std::cout << res.message << "\n";
        return 0;
    }

    std::cout << "Installing minigit (v" << minigit::core::MINIGIT_VERSION << ")...\n";
    auto res = perform_install(options);

    if (res.escalated) {
        return res.success ? 0 : 1;
    }

    if (!res.success) {
        std::cerr << "error: " << res.message << "\n";
        if (options.scope == InstallScope::System) {
            std::cerr << "hint: re-run in an elevated shell (Run as Administrator) or use 'minigit install --user' for per-user installation without elevation.\n";
        }
        return 1;
    }

    std::cout << "Installation scope: " << scope_to_string(res.effective_scope)
              << (res.effective_scope == InstallScope::System ? " (System-wide)" : " (Current User)") << "\n";
    std::cout << "Installed binary:   " << res.installed_exe.string() << "\n";
    if (options.add_to_path) {
        std::cout << "Environment PATH:   "
                  << (res.path_modified ? "Added directory to PATH" : "Directory already present in PATH") << "\n";
    }
    std::cout << "minigit installed successfully!\n";
    return 0;
}

} // namespace minigit::install
