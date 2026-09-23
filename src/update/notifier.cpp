#include "notifier.h"
#include "update.h"
#include "semver.h"
#include "core/version.h"
#include "core/logger.h"
#include "repository/repository.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#if defined(_WIN32)
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

namespace minigit::update {

UpdateNotifier& UpdateNotifier::instance() {
    static UpdateNotifier notifier;
    return notifier;
}

UpdateNotifier::UpdateNotifier() {
    const char* env_interval = std::getenv("MINIGIT_UPDATE_CHECK_INTERVAL");
    if (env_interval && *env_interval) {
        try {
            check_interval_seconds_ = std::stoi(env_interval);
        } catch (...) {
            check_interval_seconds_ = 86400;
        }
    }
}

void UpdateNotifier::set_cache_path(const std::string& path) {
    custom_cache_path_ = path;
}

void UpdateNotifier::set_check_interval_seconds(int seconds) {
    check_interval_seconds_ = seconds;
}

int UpdateNotifier::get_check_interval_seconds() const {
    return check_interval_seconds_;
}

bool UpdateNotifier::is_notification_enabled() const {
    const char* force_notifier = std::getenv("MINIGIT_FORCE_UPDATE_NOTIFIER");
    if (force_notifier && (*force_notifier == '1' || std::string(force_notifier) == "true")) {
        return true;
    }

    const char* no_notifier = std::getenv("MINIGIT_NO_UPDATE_NOTIFIER");
    if (no_notifier && (*no_notifier == '1' || std::string(no_notifier) == "true")) {
        return false;
    }

    const char* disable_check = std::getenv("MINIGIT_DISABLE_UPDATE_CHECK");
    if (disable_check && (*disable_check == '1' || std::string(disable_check) == "true")) {
        return false;
    }

    if (!ISATTY(FILENO(stderr))) {
        return false;
    }

    return true;
}

std::string UpdateNotifier::get_cache_path() const {
    if (!custom_cache_path_.empty()) {
        return custom_cache_path_;
    }

    const char* env_cache = std::getenv("MINIGIT_UPDATE_CACHE");
    if (env_cache && *env_cache) {
        return env_cache;
    }

    try {
        Repository repo = Repository::discover(std::filesystem::current_path());
        return (repo.git_dir() / "update_cache").string();
    } catch (...) {
        // Not in repo or repo lookup failed
    }

    static const std::unordered_map<std::string_view, std::string_view> kHomeEnvMap = {
        {"windows", "USERPROFILE"},
        {"macos",   "HOME"},
        {"linux",   "HOME"}
    };

    std::string_view os_key = os_to_string(get_current_os());
    auto it = kHomeEnvMap.find(os_key);
    if (it != kHomeEnvMap.end()) {
        const char* home_dir = std::getenv(it->second.data());
        if (home_dir && *home_dir) {
            std::filesystem::path dir = std::filesystem::path(home_dir) / ".minigit";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            return (dir / "update_cache").string();
        }
    }

    std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
    return (temp_dir / "minigit_update_cache").string();
}

namespace {

struct CacheData {
    uint64_t last_check{0};
    std::string latest_version;
    std::string download_url;
};

CacheData read_cache_file(const std::string& path) {
    CacheData data;
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return data;

    std::string line;
    bool first_line = true;
    while (std::getline(in, line)) {
        if (first_line) {
            first_line = false;
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }
        }
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "last_check") {
            try {
                data.last_check = std::stoull(val);
            } catch (...) {}
        } else if (key == "latest_version") {
            data.latest_version = val;
        } else if (key == "download_url") {
            data.download_url = val;
        }
    }
    return data;
}

void write_cache_file(const std::string& path, const CacheData& data) {
    std::error_code ec;
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;

    out << "last_check=" << data.last_check << "\n"
        << "latest_version=" << data.latest_version << "\n"
        << "download_url=" << data.download_url << "\n";
}

} // namespace

void UpdateNotifier::check_and_notify(std::string_view command) {
    static const std::unordered_set<std::string_view> excluded_commands = {
        "init", "version", "--version", "-v", "update",
        "hash-object", "cat-file", "write-tree", "ls-files", "ls-tree", "verify-pack", "repack"
    };

    if (excluded_commands.find(command) != excluded_commands.end()) {
        return;
    }

    if (!is_notification_enabled()) {
        return;
    }

    auto current_semver = SemVer::parse(minigit::core::MINIGIT_VERSION);
    if (!current_semver.has_value()) {
        return;
    }

    std::string cache_file = get_cache_path();
    CacheData cache = read_cache_file(cache_file);

    uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    int interval = check_interval_seconds_;
    const char* env_interval = std::getenv("MINIGIT_UPDATE_CHECK_INTERVAL");
    if (env_interval && *env_interval) {
        try {
            interval = std::stoi(env_interval);
        } catch (...) {}
    }

    if (cache.last_check > 0 && (now >= cache.last_check) && (now - cache.last_check < static_cast<uint64_t>(interval))) {
        // Cache is fresh
        if (!cache.latest_version.empty()) {
            auto latest_semver = SemVer::parse(cache.latest_version);
            if (latest_semver.has_value() && *latest_semver > *current_semver) {
                print_update_banner(minigit::core::MINIGIT_VERSION, cache.latest_version, cache.download_url);
            }
        }
        return;
    }

    // Cache expired or missing -> check for update with 2-second timeout
    std::string repo = "sagarkrjha/minigit";
    const char* env_repo = std::getenv("MINIGIT_UPDATE_REPO");
    if (env_repo && *env_repo) {
        repo = env_repo;
    }

    auto release = fetch_latest_release(repo, 2);
    if (release.has_value()) {
        cache.last_check = now;
        cache.latest_version = release->tag_name;
        cache.download_url = release->html_url;
        write_cache_file(cache_file, cache);

        if (release->version > *current_semver) {
            print_update_banner(minigit::core::MINIGIT_VERSION, release->tag_name, release->html_url);
        }
    } else {
        // Record last check to avoid repeating failed network requests on every command
        cache.last_check = now;
        write_cache_file(cache_file, cache);
    }
}

void UpdateNotifier::print_update_banner(std::string_view current_ver, std::string_view latest_ver, std::string_view url) {
    (void)url;
    std::string line1 = "A new version of minigit is available: v" + std::string(current_ver) + " -> " + std::string(latest_ver);
    std::string line2 = "Run 'minigit update' to update to the latest release";

    size_t content_len = std::max(line1.size(), line2.size());
    size_t total_width = content_len + 4; // 2 spaces on each side

    bool ascii_only = false;
    const char* env_ascii = std::getenv("MINIGIT_NO_UNICODE");
    if (env_ascii && (*env_ascii == '1' || std::string(env_ascii) == "true")) {
        ascii_only = true;
    }

    std::string top_left = ascii_only ? "+" : "┌";
    std::string top_right = ascii_only ? "+" : "┐";
    std::string bottom_left = ascii_only ? "+" : "└";
    std::string bottom_right = ascii_only ? "+" : "┘";
    std::string horiz = ascii_only ? "-" : "─";
    std::string vert = ascii_only ? "|" : "│";

    std::string border;
    for (size_t i = 0; i < total_width; ++i) {
        border += horiz;
    }

    auto pad_line = [&](const std::string& line) {
        size_t pad_right = total_width - line.size() - 2;
        return vert + "  " + line + std::string(pad_right, ' ') + vert;
    };

    std::cerr << "\n"
              << top_left << border << top_right << "\n"
              << pad_line(line1) << "\n"
              << pad_line(line2) << "\n"
              << bottom_left << border << bottom_right << "\n\n";
}

} // namespace minigit::update
