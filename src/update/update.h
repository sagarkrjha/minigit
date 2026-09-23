#pragma once

#include "semver.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace minigit::update {

enum class OperatingSystem {
    Windows,
    Linux,
    MacOS,
    Unknown
};

OperatingSystem get_current_os();
std::string_view os_to_string(OperatingSystem os);

struct ReleaseAsset {
    std::string name;
    std::string download_url;
    size_t size{0};
};

struct ReleaseInfo {
    std::string tag_name;
    std::string title;
    std::string body;
    std::string html_url;
    std::string published_at;
    SemVer version;
    std::vector<ReleaseAsset> assets;

    const ReleaseAsset* find_platform_asset() const;
};

std::string get_current_platform_asset_name();
std::filesystem::path get_executable_path();
bool replace_executable(const std::filesystem::path& target_path, const std::filesystem::path& new_path);

std::optional<ReleaseInfo> parse_release_json(std::string_view json_str);

std::optional<ReleaseInfo> fetch_latest_release(
    const std::string& repo = "sagarkrjha/minigit",
    long timeout_seconds = 10
);

bool is_update_available(const ReleaseInfo& release, const SemVer& current_version);

int update_command(int argc, char const *argv[]);

} // namespace minigit::update
