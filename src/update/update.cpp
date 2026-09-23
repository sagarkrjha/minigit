#include "update.h"
#include "json.h"
#include "notifier.h"
#include "core/version.h"
#include "core/logger.h"
#include "install/install.h"
#include "remotes/http_client.h"

#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/stat.h>
#elif defined(__linux__)
#include <unistd.h>
#include <sys/stat.h>
#include <climits>
#endif

namespace minigit::update {

OperatingSystem get_current_os() {
    const char* override_os = std::getenv("MINIGIT_OS_OVERRIDE");
    if (override_os) {
        static const std::unordered_map<std::string_view, OperatingSystem> kOsOverrideMap = {
            {"windows", OperatingSystem::Windows},
            {"win",     OperatingSystem::Windows},
            {"macos",   OperatingSystem::MacOS},
            {"darwin",  OperatingSystem::MacOS},
            {"apple",   OperatingSystem::MacOS},
            {"linux",   OperatingSystem::Linux}
        };
        auto it = kOsOverrideMap.find(override_os);
        if (it != kOsOverrideMap.end()) {
            return it->second;
        }
    }

#if defined(_WIN32)
    return OperatingSystem::Windows;
#elif defined(__APPLE__)
    return OperatingSystem::MacOS;
#elif defined(__linux__)
    return OperatingSystem::Linux;
#else
    return OperatingSystem::Unknown;
#endif
}

std::string_view os_to_string(OperatingSystem os) {
    static const std::unordered_map<OperatingSystem, std::string_view> kOsNameMap = {
        {OperatingSystem::Windows, "windows"},
        {OperatingSystem::MacOS,   "macos"},
        {OperatingSystem::Linux,   "linux"},
        {OperatingSystem::Unknown, "unknown"}
    };
    auto it = kOsNameMap.find(os);
    return it != kOsNameMap.end() ? it->second : "unknown";
}

std::string get_current_platform_asset_name() {
    const char* env_asset = std::getenv("MINIGIT_UPDATE_ASSET");
    if (env_asset && *env_asset) {
        return env_asset;
    }

    static const std::unordered_map<std::string_view, std::string_view> kPlatformBinaryMap = {
        {"windows", "minigit.exe"},
        {"macos",   "minigit-macos"},
        {"linux",   "minigit-linux"}
    };

    std::string_view os_key = os_to_string(get_current_os());
    auto it = kPlatformBinaryMap.find(os_key);
    if (it != kPlatformBinaryMap.end()) {
        return std::string(it->second);
    }
    return "minigit";
}

using AssetMatcher = std::function<bool(const std::string&)>;

static const std::unordered_map<std::string_view, AssetMatcher> kOsAssetMatchers = {
    {"windows", [](const std::string& name) {
        return name.ends_with(".exe") || name.find("windows") != std::string::npos || name.find("win") != std::string::npos;
    }},
    {"macos", [](const std::string& name) {
        return name.find("macos") != std::string::npos || name.find("darwin") != std::string::npos || name.find("apple") != std::string::npos;
    }},
    {"linux", [](const std::string& name) {
        return name.find("linux") != std::string::npos;
    }}
};

const ReleaseAsset* ReleaseInfo::find_platform_asset() const {
    std::string expected_name = get_current_platform_asset_name();
    for (const auto& asset : assets) {
        if (asset.name == expected_name) {
            return &asset;
        }
    }

    // Fallback matching heuristics via unordered_map lookup
    std::string_view os_key = os_to_string(get_current_os());
    auto it = kOsAssetMatchers.find(os_key);
    if (it != kOsAssetMatchers.end()) {
        for (const auto& asset : assets) {
            if (it->second(asset.name)) {
                return &asset;
            }
        }
    }

    return nullptr;
}

namespace {

using ExecutablePathResolver = std::function<std::filesystem::path()>;

std::filesystem::path resolve_windows_executable() {
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
#endif
    return {};
}

std::filesystem::path resolve_macos_executable() {
#if defined(__APPLE__)
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
#endif
    return {};
}

std::filesystem::path resolve_linux_executable() {
#if defined(__linux__)
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

using ExecutableReplacer = std::function<bool(const std::filesystem::path&, const std::filesystem::path&)>;

bool replace_windows_executable(const std::filesystem::path& target_path, const std::filesystem::path& new_path) {
#if defined(_WIN32)
    std::filesystem::path old_path = target_path;
    old_path += ".old";
    std::error_code ec;
    if (std::filesystem::exists(old_path, ec)) {
        std::filesystem::remove(old_path, ec);
    }

    std::filesystem::rename(target_path, old_path, ec);
    if (ec) {
        LOG_DEBUG("update", "failed to rename existing executable to .old: " << ec.message());
        return false;
    }

    std::filesystem::rename(new_path, target_path, ec);
    if (ec) {
        LOG_DEBUG("update", "failed to rename new binary to target, rolling back: " << ec.message());
        std::filesystem::rename(old_path, target_path, ec);
        return false;
    }

    std::filesystem::remove(old_path, ec);
    return true;
#else
    (void)target_path; (void)new_path;
    return false;
#endif
}

bool replace_posix_executable(const std::filesystem::path& target_path, const std::filesystem::path& new_path) {
#if !defined(_WIN32)
    std::error_code ec;
    std::filesystem::permissions(new_path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec |
        std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
        std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
        std::filesystem::perm_options::replace, ec);

    std::filesystem::rename(new_path, target_path, ec);
    if (ec) {
        LOG_DEBUG("update", "failed to rename new binary to target: " << ec.message());
        return false;
    }
    return true;
#else
    (void)target_path; (void)new_path;
    return false;
#endif
}

} // namespace

std::filesystem::path get_executable_path() {
    const char* env_path = std::getenv("MINIGIT_EXEC_PATH_OVERRIDE");
    if (env_path && *env_path) {
        return std::filesystem::path(env_path);
    }

    static const std::unordered_map<std::string_view, ExecutablePathResolver> kExecResolvers = {
        {"windows", resolve_windows_executable},
        {"macos",   resolve_macos_executable},
        {"linux",   resolve_linux_executable}
    };

    std::string_view os_key = os_to_string(get_current_os());
    auto it = kExecResolvers.find(os_key);
    if (it != kExecResolvers.end()) {
        auto path = it->second();
        if (!path.empty()) return path;
    }

    return {};
}

bool replace_executable(const std::filesystem::path& target_path, const std::filesystem::path& new_path) {
    static const std::unordered_map<std::string_view, ExecutableReplacer> kReplacers = {
        {"windows", replace_windows_executable},
        {"macos",   replace_posix_executable},
        {"linux",   replace_posix_executable}
    };

    std::string_view os_key = os_to_string(get_current_os());
    auto it = kReplacers.find(os_key);
    if (it != kReplacers.end()) {
        return it->second(target_path, new_path);
    }
    return false;
}

std::optional<ReleaseInfo> parse_release_json(std::string_view json_str) {
    JsonValue root = JsonValue::parse(json_str);
    if (!root.is_object()) {
        return std::nullopt;
    }

    std::string tag_name = root.get_string("tag_name");
    if (tag_name.empty()) {
        return std::nullopt;
    }

    auto semver = SemVer::parse(tag_name);
    if (!semver.has_value()) {
        return std::nullopt;
    }

    ReleaseInfo info;
    info.tag_name = std::move(tag_name);
    info.version = *semver;
    info.title = root.get_string("name");
    info.body = root.get_string("body");
    info.html_url = root.get_string("html_url");
    info.published_at = root.get_string("published_at");

    const auto& assets_val = root["assets"];
    if (assets_val.is_array()) {
        for (const auto& item : assets_val.as_array()) {
            if (!item.is_object()) continue;
            ReleaseAsset asset;
            asset.name = item.get_string("name");
            asset.download_url = item.get_string("browser_download_url");
            asset.size = static_cast<size_t>(item.get_number("size"));
            if (!asset.name.empty() && !asset.download_url.empty()) {
                info.assets.push_back(std::move(asset));
            }
        }
    }

    return info;
}

std::optional<ReleaseInfo> fetch_latest_release(const std::string& repo, long timeout_seconds) {
    std::string url;
    const char* env_url = std::getenv("MINIGIT_UPDATE_URL");
    if (env_url && *env_url) {
        url = env_url;
    } else {
        url = "https://api.github.com/repos/" + repo + "/releases/latest";
    }

    LOG_DEBUG("update", "fetching release info from " << url);

    minigit::remotes::HttpClient client;
    client.set_timeout_seconds(timeout_seconds);

    std::string user_agent = "minigit/" + std::string(minigit::core::MINIGIT_VERSION);
    std::vector<std::string> headers = {
        "User-Agent: " + user_agent,
        "Accept: application/vnd.github.v3+json"
    };

    auto response = client.get(url, headers);
    if (!response.ok()) {
        LOG_DEBUG("update", "fetch release failed with status " << response.status_code << ", error: " << response.error);
        return std::nullopt;
    }

    return parse_release_json(response.body);
}

bool is_update_available(const ReleaseInfo& release, const SemVer& current_version) {
    return release.version > current_version;
}

namespace {

void print_usage() {
    std::cout << "usage: minigit update [--check] [--force] [--repo <owner/repo>]\n\n"
              << "Check for updates and update minigit to the latest version.\n\n"
              << "Options:\n"
              << "  --check          Check for updates without downloading or installing\n"
              << "  -f, --force      Force update even if minigit is already at latest version\n"
              << "  --repo <repo>    GitHub repository to check (default: sagarkrjha/minigit)\n"
              << "  -h, --help       Show this help message\n";
}

} // namespace

int update_command(int argc, char const *argv[]) {
    bool check_only = false;
    bool force = false;
    std::string repo = "sagarkrjha/minigit";

    const char* env_repo = std::getenv("MINIGIT_UPDATE_REPO");
    if (env_repo && *env_repo) {
        repo = env_repo;
    }

    for (int i = 2; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--check") {
            check_only = true;
        } else if (arg == "-f" || arg == "--force") {
            force = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage();
            return 0;
        } else if (arg == "--repo") {
            if (i + 1 < argc) {
                repo = argv[++i];
            } else {
                std::cerr << "error: --repo requires a repository argument (e.g. owner/repo)\n";
                return 1;
            }
        } else {
            std::cerr << "error: unknown argument '" << arg << "'\n";
            print_usage();
            return 1;
        }
    }

    auto current_ver = SemVer::parse(minigit::core::MINIGIT_VERSION);
    if (!current_ver.has_value()) {
        std::cerr << "error: failed to parse current minigit version\n";
        return 1;
    }

    std::cout << "Checking for latest release from " << repo << "...\n";
    auto release = fetch_latest_release(repo, 15);
    if (!release.has_value()) {
        std::cerr << "error: failed to check for updates (network or release discovery error)\n";
        return 1;
    }

    bool newer = is_update_available(*release, *current_ver);

    if (check_only) {
        if (newer) {
            std::cout << "A new version of minigit is available: v" << current_ver->to_string()
                      << " -> " << release->tag_name << "\n";
            if (!release->title.empty()) {
                std::cout << "Release: " << release->title << "\n";
            }
            if (!release->published_at.empty()) {
                std::cout << "Published: " << release->published_at << "\n";
            }
            if (!release->html_url.empty()) {
                std::cout << "Release URL: " << release->html_url << "\n";
            }
            std::cout << "\nRun 'minigit update' to upgrade.\n";
        } else {
            std::cout << "minigit is already up to date (v" << current_ver->to_string() << ").\n";
        }
        return 0;
    }

    if (!newer && !force) {
        std::cout << "minigit is already up to date (v" << current_ver->to_string() << ").\n";
        return 0;
    }

    const ReleaseAsset* asset = release->find_platform_asset();
    if (!asset) {
        std::cerr << "error: no compatible binary asset ('" << get_current_platform_asset_name()
                  << "') found in release " << release->tag_name << "\n";
        return 1;
    }

    std::filesystem::path exe_path = get_executable_path();
    if (exe_path.empty()) {
        std::cerr << "error: unable to determine minigit executable path\n";
        return 1;
    }

    // Pre-flight write permission check for binary destination
    bool writable = minigit::install::can_write_to_directory(exe_path.parent_path()) &&
                    minigit::install::can_write_to_file(exe_path);
    if (!writable) {
        std::cout << "Write permissions required to update minigit at " << exe_path.string() << ".\n";
#if defined(_WIN32)
        std::cout << "Requesting Administrator privileges via Windows UAC...\n";
        std::vector<std::string> args = {"update"};
        if (force) args.push_back("--force");
        if (repo != "sagarkrjha/minigit") {
            args.push_back("--repo");
            args.push_back(repo);
        }
        int exit_code = 0;
        if (minigit::install::request_uac_elevation(exe_path, args, exit_code)) {
            return exit_code;
        }
        std::cerr << "error: administrator privileges required to update " << exe_path.string()
                  << ". Please run from an elevated command prompt (Run as Administrator) or approve the UAC prompt.\n";
        return 1;
#else
        std::cerr << "error: write permission denied to update " << exe_path.string()
                  << ". Please re-run with sudo: sudo minigit update\n";
        return 1;
#endif
    }

    std::cout << "Found " << release->tag_name << " (current: v" << current_ver->to_string() << ")\n";
    std::cout << "Downloading " << asset->name << " from " << asset->download_url << "...\n";

    minigit::remotes::HttpClient download_client;
    download_client.set_timeout_seconds(120);

    std::string user_agent = "minigit/" + std::string(minigit::core::MINIGIT_VERSION);
    std::vector<std::string> headers = {
        "User-Agent: " + user_agent,
        "Accept: application/octet-stream"
    };

    auto dl_response = download_client.get(asset->download_url, headers);
    if (!dl_response.ok() || dl_response.body.empty()) {
        std::cerr << "error: failed to download asset from " << asset->download_url;
        if (!dl_response.error.empty()) {
            std::cerr << " (" << dl_response.error << ")";
        }
        std::cerr << "\n";
        return 1;
    }

    std::filesystem::path tmp_path = exe_path;
    tmp_path += ".tmp";

    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "error: unable to create temporary file at " << tmp_path.string() << "\n";
            return 1;
        }
        out.write(dl_response.body.data(), dl_response.body.size());
        out.flush();
    }

    std::cout << "Replacing executable at " << exe_path.string() << "...\n";
    if (!replace_executable(exe_path, tmp_path)) {
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        std::cerr << "error: failed to replace executable at " << exe_path.string() << "\n";
        return 1;
    }

    std::cout << "Successfully updated minigit to " << release->tag_name << "!\n";
    std::cout << "Updated binary: " << exe_path.string() << "\n";

    // Update notification cache so notifier knows we're up to date
    UpdateNotifier::instance().set_cache_path(""); // default
    // We can also record successful update in cache

    return 0;
}

} // namespace minigit::update
