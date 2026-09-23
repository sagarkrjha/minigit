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
        return base / "MiniGit";
#else
        return std::filesystem::path("/usr/local");
#endif
    } else { // User or Auto default
#if defined(_WIN32)
        const char* localappdata = std::getenv("LOCALAPPDATA");
        if (localappdata && *localappdata) {
            return std::filesystem::path(localappdata) / "Programs" / "MiniGit";
        }
        const char* userprofile = std::getenv("USERPROFILE");
        if (userprofile && *userprofile) {
            return std::filesystem::path(userprofile) / ".minigit";
        }
        return std::filesystem::path("C:\\MiniGit");
#else
        const char* home = std::getenv("HOME");
        if (home && *home) {
            return std::filesystem::path(home) / ".local";
        }
        return std::filesystem::path("/usr/local");
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

// Mock flags for non-intrusive unit testing
bool g_mock_uninstall_registered = false;
bool g_mock_context_menu_registered = false;

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

bool register_windows_uninstall(const std::filesystem::path& install_root,
                                const std::filesystem::path& cmd_exe,
                                InstallScope scope) {
    if (std::getenv("MINIGIT_MOCK_REGISTRY")) {
        g_mock_uninstall_registered = true;
        return true;
    }

#if defined(_WIN32)
    HKEY hRoot = (scope == InstallScope::System) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* subkey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MiniGit";

    HKEY hKey = NULL;
    DWORD disposition = 0;
    LONG res = RegCreateKeyExW(hRoot, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &disposition);
    if (res != ERROR_SUCCESS) {
        LOG_DEBUG("install", "failed to create uninstall key: " << res);
        return false;
    }

    auto set_string_val = [&](const wchar_t* name, const std::wstring& val) {
        RegSetValueExW(hKey, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(val.c_str()),
                       static_cast<DWORD>((val.size() + 1) * sizeof(wchar_t)));
    };

    auto set_dword_val = [&](const wchar_t* name, DWORD val) {
        RegSetValueExW(hKey, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(DWORD));
    };

    std::wstring w_name = L"MiniGit";
    std::wstring w_version = utf8_to_wstring(minigit::core::MINIGIT_VERSION);
    std::wstring w_publisher = L"Sagar Kumar Jha";
    std::wstring w_loc = install_root.wstring();
    std::wstring w_uninst = L"\"" + cmd_exe.wstring() + L"\" install --uninstall";
    std::wstring w_icon = cmd_exe.wstring() + L",0";
    std::wstring w_url = L"https://github.com/sagarkrjha/minigit";
    std::wstring w_help = L"https://github.com/sagarkrjha/minigit/blob/main/USAGE.md";

    set_string_val(L"DisplayName", w_name);
    set_string_val(L"DisplayVersion", w_version);
    set_string_val(L"Publisher", w_publisher);
    set_string_val(L"InstallLocation", w_loc);
    set_string_val(L"UninstallString", w_uninst);
    set_string_val(L"QuietUninstallString", w_uninst);
    set_string_val(L"DisplayIcon", w_icon);
    set_string_val(L"URLInfoAbout", w_url);
    set_string_val(L"HelpLink", w_help);
    set_dword_val(L"NoModify", 1);
    set_dword_val(L"NoRepair", 1);

    // Calculate approximate size in KB
    DWORD size_kb = 40960; // ~40MB typical
    set_dword_val(L"EstimatedSize", size_kb);

    RegCloseKey(hKey);
    return true;
#else
    (void)install_root; (void)cmd_exe; (void)scope;
    return true;
#endif
}

bool unregister_windows_uninstall(InstallScope scope) {
    if (std::getenv("MINIGIT_MOCK_REGISTRY")) {
        g_mock_uninstall_registered = false;
        return true;
    }

#if defined(_WIN32)
    HKEY hRoot = (scope == InstallScope::System) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* subkey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MiniGit";
    LONG res = RegDeleteKeyW(hRoot, subkey);
    return (res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND);
#else
    (void)scope;
    return true;
#endif
}

bool is_windows_uninstall_registered(InstallScope scope) {
    if (std::getenv("MINIGIT_MOCK_REGISTRY")) {
        return g_mock_uninstall_registered;
    }

#if defined(_WIN32)
    HKEY hRoot = (scope == InstallScope::System) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const wchar_t* subkey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MiniGit";
    HKEY hKey = NULL;
    if (RegOpenKeyExW(hRoot, subkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
#else
    (void)scope;
    return false;
#endif
}

bool register_explorer_context_menu(const std::filesystem::path& cmd_exe, InstallScope scope) {
    if (std::getenv("MINIGIT_MOCK_REGISTRY")) {
        g_mock_context_menu_registered = true;
        return true;
    }

#if defined(_WIN32)
    HKEY hRoot = (scope == InstallScope::System) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    auto create_menu_entry = [&](const wchar_t* shell_key_path, const std::wstring& cmd_str) -> bool {
        HKEY hKey = NULL;
        if (RegCreateKeyExW(hRoot, shell_key_path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL) != ERROR_SUCCESS) {
            return false;
        }
        std::wstring title = L"Open MiniGit Prompt Here";
        RegSetValueExW(hKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(title.c_str()),
                       static_cast<DWORD>((title.size() + 1) * sizeof(wchar_t)));
        std::wstring icon = cmd_exe.wstring();
        RegSetValueExW(hKey, L"Icon", 0, REG_SZ, reinterpret_cast<const BYTE*>(icon.c_str()),
                       static_cast<DWORD>((icon.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);

        std::wstring cmd_sub = std::wstring(shell_key_path) + L"\\command";
        HKEY hCmd = NULL;
        if (RegCreateKeyExW(hRoot, cmd_sub.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hCmd, NULL) != ERROR_SUCCESS) {
            return false;
        }
        RegSetValueExW(hCmd, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd_str.c_str()),
                       static_cast<DWORD>((cmd_str.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hCmd);
        return true;
    };

    // Context menu for folder background
    bool ok1 = create_menu_entry(L"Software\\Classes\\Directory\\Background\\shell\\MiniGit",
                                 L"cmd.exe /s /k pushd \"%V\"");
    // Context menu for clicking on a folder directory directly
    bool ok2 = create_menu_entry(L"Software\\Classes\\Directory\\shell\\MiniGit",
                                 L"cmd.exe /s /k pushd \"%1\"");
    return ok1 && ok2;
#else
    (void)cmd_exe; (void)scope;
    return true;
#endif
}

bool unregister_explorer_context_menu(InstallScope scope) {
    if (std::getenv("MINIGIT_MOCK_REGISTRY")) {
        g_mock_context_menu_registered = false;
        return true;
    }

#if defined(_WIN32)
    HKEY hRoot = (scope == InstallScope::System) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    RegDeleteKeyW(hRoot, L"Software\\Classes\\Directory\\Background\\shell\\MiniGit\\command");
    RegDeleteKeyW(hRoot, L"Software\\Classes\\Directory\\Background\\shell\\MiniGit");
    RegDeleteKeyW(hRoot, L"Software\\Classes\\Directory\\shell\\MiniGit\\command");
    RegDeleteKeyW(hRoot, L"Software\\Classes\\Directory\\shell\\MiniGit");
    return true;
#else
    (void)scope;
    return true;
#endif
}

bool is_explorer_context_menu_registered(InstallScope scope) {
    if (std::getenv("MINIGIT_MOCK_REGISTRY")) {
        return g_mock_context_menu_registered;
    }

#if defined(_WIN32)
    HKEY hRoot = (scope == InstallScope::System) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    HKEY hKey = NULL;
    if (RegOpenKeyExW(hRoot, L"Software\\Classes\\Directory\\Background\\shell\\MiniGit", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
#else
    (void)scope;
    return false;
#endif
}

bool setup_system_config(const std::filesystem::path& install_root) {
    std::filesystem::path etc_dir = install_root / "etc";
    std::error_code ec;
    std::filesystem::create_directories(etc_dir, ec);
    if (ec) return false;

    std::filesystem::path config_file = etc_dir / "minigitconfig";
    if (!std::filesystem::exists(config_file, ec)) {
        std::ofstream out(config_file);
        if (!out) return false;
        out << "# MiniGit System Configuration (v" << minigit::core::MINIGIT_VERSION << ")\n"
            << "[core]\n"
            << "    autocrlf = true\n"
            << "    filemode = false\n"
            << "    symlinks = false\n";
    }

    std::filesystem::path tpl_dir = etc_dir / "templates";
    std::filesystem::create_directories(tpl_dir, ec);
    return true;
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

    std::filesystem::path install_root = options.custom_dir.empty()
        ? get_default_install_dir(effective_scope)
        : options.custom_dir;
    result.install_root = install_root;

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
    if (!can_write_to_directory(install_root)) {
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
            if (!options.add_context_menu) args.push_back("--no-context-menu");
            if (options.force) args.push_back("--force");

            int exit_code = 0;
            if (request_uac_elevation(src, args, exit_code)) {
                result.success = (exit_code == 0);
                result.escalated = true;
                result.installed_cmd_exe = install_root / "cmd" / src.filename();
                result.installed_bin_exe = install_root / "bin" / src.filename();
                result.installed_exe = result.installed_cmd_exe;
                if (result.success) {
                    result.message = "Successfully installed minigit via elevated Administrator privileges";
                } else {
                    result.message = "Elevated installation failed with exit code " + std::to_string(exit_code);
                }
                return result;
            }
#endif
            result.message = "Administrator privileges required to install to " + install_root.string() +
                             ". Permission denied or elevation prompt declined.";
            return result;
        } else {
            result.message = "Permission denied: unable to write to " + install_root.string();
            return result;
        }
    }

    // Git-style Directory Layout:
    // <install_root>/cmd/minigit.exe  (added to PATH, identical to Git for Windows C:\Program Files\Git\cmd)
    // <install_root>/bin/minigit.exe  (core executable)
    // <install_root>/etc/minigitconfig (system config)
    std::filesystem::path cmd_dir = install_root / "cmd";
    std::filesystem::path bin_dir = install_root / "bin";

    std::filesystem::create_directories(cmd_dir, ec);
    std::filesystem::create_directories(bin_dir, ec);
    if (ec) {
        result.message = "Failed to create installation directories: " + ec.message();
        return result;
    }

    std::string exe_filename = src.filename().string();
#if defined(_WIN32)
    if (!exe_filename.ends_with(".exe")) {
        exe_filename += ".exe";
    }
#endif

    std::filesystem::path target_cmd_exe = cmd_dir / exe_filename;
    std::filesystem::path target_bin_exe = bin_dir / exe_filename;
    result.installed_cmd_exe = target_cmd_exe;
    result.installed_bin_exe = target_bin_exe;
    result.installed_exe = target_cmd_exe;

    // Check if source executable is already installed
    if (!options.force) {
        auto can_src = std::filesystem::weakly_canonical(src, ec);
        bool same_as_cmd = false;
        bool same_as_bin = false;
        if (std::filesystem::exists(target_cmd_exe, ec)) {
            auto can_cmd = std::filesystem::weakly_canonical(target_cmd_exe, ec);
            if (!ec && can_src == can_cmd) {
                same_as_cmd = true;
            }
        }
        if (std::filesystem::exists(target_bin_exe, ec)) {
            auto can_bin = std::filesystem::weakly_canonical(target_bin_exe, ec);
            if (!ec && can_src == can_bin) {
                same_as_bin = true;
            }
        }
        if (same_as_cmd || same_as_bin) {
            result.success = true;
            result.message = "minigit is already installed at " + install_root.string();
            if (options.add_to_path) {
                result.path_modified = add_directory_to_env_path(cmd_dir, effective_scope);
            }
            return result;
        }
    }

    auto copy_or_replace = [&](const std::filesystem::path& target) -> bool {
        if (std::filesystem::exists(target, ec)) {
            auto can_src = std::filesystem::weakly_canonical(src, ec);
            auto can_tgt = std::filesystem::weakly_canonical(target, ec);
            if (can_src == can_tgt) {
                return true;
            }
#if defined(_WIN32)
            std::filesystem::path old_file = target;
            old_file += ".old";
            if (std::filesystem::exists(old_file, ec)) {
                std::filesystem::remove(old_file, ec);
            }
            std::filesystem::rename(target, old_file, ec);
            if (ec) return false;
            std::filesystem::copy_file(src, target, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                std::filesystem::rename(old_file, target, ec);
                return false;
            }
            std::filesystem::remove(old_file, ec);
#else
            std::filesystem::copy_file(src, target, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) return false;
#endif
        } else {
            std::filesystem::copy_file(src, target, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) return false;
        }

#if !defined(_WIN32)
        std::filesystem::permissions(target,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec |
            std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
            std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
            std::filesystem::perm_options::replace, ec);
#endif
        return true;
    };

    if (!copy_or_replace(target_cmd_exe)) {
        result.message = "Failed to copy executable to " + target_cmd_exe.string();
        return result;
    }

    if (!copy_or_replace(target_bin_exe)) {
        result.message = "Failed to copy executable to " + target_bin_exe.string();
        return result;
    }

    // Set up Git-style system config file (<install_root>/etc/minigitconfig)
    setup_system_config(install_root);

    // Environment PATH registration:
    // Exactly like Git for Windows, add <install_root>/cmd to PATH
    if (options.add_to_path) {
        // Also remove legacy <install_root>/bin if it was previously registered
        remove_directory_from_env_path(bin_dir, effective_scope);
        result.path_modified = add_directory_to_env_path(cmd_dir, effective_scope);
    }

    // Windows Add/Remove Programs (Apps & Features) integration
    result.uninstall_registered = register_windows_uninstall(install_root, target_cmd_exe, effective_scope);

    // Windows Explorer Context Menu integration
    if (options.add_context_menu) {
        result.context_menu_registered = register_explorer_context_menu(target_cmd_exe, effective_scope);
    }

    result.success = true;
    result.message = "Successfully installed minigit to " + install_root.string();
    return result;
}

InstallResult perform_uninstall(const InstallOptions& options) {
    InstallResult result;

    InstallScope effective_scope = options.scope;
    if (effective_scope == InstallScope::Auto) {
        effective_scope = is_running_as_admin() ? InstallScope::System : InstallScope::User;
    }
    result.effective_scope = effective_scope;

    std::filesystem::path install_root = options.custom_dir.empty()
        ? get_default_install_dir(effective_scope)
        : options.custom_dir;
    result.install_root = install_root;

    std::filesystem::path cmd_dir = install_root / "cmd";
    std::filesystem::path bin_dir = install_root / "bin";

    if (!can_write_to_directory(install_root)) {
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
        result.message = "Administrator privileges required to uninstall from " + install_root.string();
        return result;
    }

    std::string exe_name = "minigit";
#if defined(_WIN32)
    exe_name += ".exe";
#endif
    result.installed_cmd_exe = cmd_dir / exe_name;
    result.installed_bin_exe = bin_dir / exe_name;
    result.installed_exe = result.installed_cmd_exe;

    std::error_code ec;

    // Remove binaries in cmd and bin
    std::filesystem::remove(cmd_dir / exe_name, ec);
    std::filesystem::remove(bin_dir / exe_name, ec);

    // Also remove without .exe if present
    std::filesystem::remove(cmd_dir / "minigit", ec);
    std::filesystem::remove(bin_dir / "minigit", ec);

    // Remove direct binary if legacy root install was used
    std::filesystem::remove(install_root / exe_name, ec);
    std::filesystem::remove(install_root / "minigit", ec);

    // Remove etc configuration
    std::filesystem::remove(install_root / "etc" / "minigitconfig", ec);
    std::filesystem::remove(install_root / "etc" / "templates", ec);
    std::filesystem::remove(install_root / "etc", ec);

    // Remove empty cmd and bin subdirectories
    std::filesystem::remove(cmd_dir, ec);
    std::filesystem::remove(bin_dir, ec);

    // Remove install root if now empty
    std::filesystem::remove(install_root, ec);

    // Unregister PATH entries (<install_root>/cmd and legacy <install_root>/bin)
    if (options.add_to_path) {
        remove_directory_from_env_path(cmd_dir, effective_scope);
        remove_directory_from_env_path(bin_dir, effective_scope);
        remove_directory_from_env_path(install_root, effective_scope);
        result.path_modified = true;
    }

    // Unregister Windows Add/Remove Programs
    unregister_windows_uninstall(effective_scope);
    result.uninstall_registered = false;

    // Unregister Windows Explorer Context Menu
    unregister_explorer_context_menu(effective_scope);
    result.context_menu_registered = false;

    result.success = true;
    result.message = "Successfully uninstalled minigit from " + install_root.string();
    return result;
}

namespace {

void print_install_usage() {
    std::cout << "usage: minigit install [--system | --user] [--dir <path>] [--no-path] [--no-context-menu] [-f | --force] [--uninstall]\n\n"
              << "Install or uninstall minigit with Git-compatible directory layout, PATH registration, and Windows integration.\n\n"
              << "Options:\n"
              << "  --system             Install system-wide for all users (requires Administrator privileges)\n"
              << "  --user               Install for the current user only (no elevation required)\n"
              << "  --dir <path>         Specify custom destination directory\n"
              << "  --no-path            Do not modify the environment PATH\n"
              << "  --no-context-menu    Do not register Windows Explorer context menu\n"
              << "  -f, --force          Reinstall/overwrite existing installation\n"
              << "  --uninstall          Uninstall minigit, remove from PATH, and clean registry\n"
              << "  -h, --help           Show this help message\n";
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
        } else if (arg == "--no-context-menu") {
            options.add_context_menu = false;
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

    std::cout << "Installation scope:  " << scope_to_string(res.effective_scope)
              << (res.effective_scope == InstallScope::System ? " (System-wide)" : " (Current User)") << "\n";
    std::cout << "Destination root:    " << res.install_root.string() << "\n";
    std::cout << "CLI command binary:  " << res.installed_cmd_exe.string() << "\n";
    std::cout << "Core binary:         " << res.installed_bin_exe.string() << "\n";
    if (options.add_to_path) {
        std::cout << "Environment PATH:    "
                  << (res.path_modified ? "Added <InstallDir>\\cmd to PATH" : "Directory already present in PATH") << "\n";
    }
#if defined(_WIN32)
    std::cout << "Apps & Features:     "
              << (res.uninstall_registered ? "Registered in Windows Installed Apps" : "Skipped") << "\n";
    if (options.add_context_menu) {
        std::cout << "Explorer Menu:       "
                  << (res.context_menu_registered ? "Registered 'Open MiniGit Prompt Here'" : "Skipped") << "\n";
    }
#endif
    std::cout << "minigit installed successfully!\n";
    return 0;
}

} // namespace minigit::install
