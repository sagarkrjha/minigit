#include "test_framework.h"
#include "update/json.h"
#include "update/semver.h"
#include "update/update.h"
#include "update/notifier.h"
#include "core/version.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using namespace minigit::update;

namespace {

fs::path make_temp_test_dir(const std::string& prefix) {
    auto p = fs::temp_directory_path() /
        ("minigit_update_test_" + prefix + "_" +
         std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    fs::create_directories(p);
    return p;
}

void remove_test_dir(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

} // namespace

TEST_CASE(Update, SemVerParsing) {
    auto v1 = SemVer::parse("1.10.0");
    ASSERT_TRUE(v1.has_value());
    ASSERT_EQ(v1->major, 1);
    ASSERT_EQ(v1->minor, 10);
    ASSERT_EQ(v1->patch, 0);
    ASSERT_TRUE(v1->prerelease.empty());
    ASSERT_EQ(v1->to_string(), "1.10.0");

    auto v2 = SemVer::parse("v1.10.2");
    ASSERT_TRUE(v2.has_value());
    ASSERT_EQ(v2->major, 1);
    ASSERT_EQ(v2->minor, 10);
    ASSERT_EQ(v2->patch, 2);
    ASSERT_EQ(v2->to_string(), "1.10.2");

    auto v3 = SemVer::parse("V2.0.0-beta.1");
    ASSERT_TRUE(v3.has_value());
    ASSERT_EQ(v3->major, 2);
    ASSERT_EQ(v3->minor, 0);
    ASSERT_EQ(v3->patch, 0);
    ASSERT_EQ(v3->prerelease, "beta.1");
    ASSERT_EQ(v3->to_string(), "2.0.0-beta.1");

    auto v4 = SemVer::parse("  v0.9.5  ");
    ASSERT_TRUE(v4.has_value());
    ASSERT_EQ(v4->major, 0);
    ASSERT_EQ(v4->minor, 9);
    ASSERT_EQ(v4->patch, 5);

    // Invalid parses
    ASSERT_FALSE(SemVer::parse("").has_value());
    ASSERT_FALSE(SemVer::parse("v").has_value());
    ASSERT_FALSE(SemVer::parse("invalid").has_value());
    ASSERT_FALSE(SemVer::parse("1.2.3.4").has_value());
    ASSERT_FALSE(SemVer::parse("1.a.3").has_value());
}

TEST_CASE(Update, SemVerComparison) {
    auto v1_9_0 = SemVer::parse("1.9.0");
    auto v1_10_0 = SemVer::parse("1.10.0");
    auto v1_10_1 = SemVer::parse("1.10.1");
    auto v1_11_0 = SemVer::parse("1.11.0");
    auto v2_0_0 = SemVer::parse("2.0.0");
    auto v2_0_0_rc = SemVer::parse("2.0.0-rc1");
    auto v2_0_0_beta = SemVer::parse("2.0.0-beta");

    ASSERT_TRUE(*v1_10_0 > *v1_9_0);
    ASSERT_TRUE(*v1_9_0 < *v1_10_0);
    ASSERT_TRUE(*v1_10_1 > *v1_10_0);
    ASSERT_TRUE(*v1_11_0 > *v1_10_1);
    ASSERT_TRUE(*v2_0_0 > *v1_11_0);

    // Equality
    auto v1_10_0_alt = SemVer::parse("v1.10.0");
    ASSERT_TRUE(*v1_10_0 == *v1_10_0_alt);
    ASSERT_FALSE(*v1_10_0 != *v1_10_0_alt);

    // Prerelease comparison: release > prerelease
    ASSERT_TRUE(*v2_0_0 > *v2_0_0_rc);
    ASSERT_TRUE(*v2_0_0_rc < *v2_0_0);
    ASSERT_TRUE(*v2_0_0_rc > *v2_0_0_beta);
}

TEST_CASE(Update, JsonParserPrimitives) {
    auto val_null = JsonValue::parse("null");
    ASSERT_TRUE(val_null.is_null());

    auto val_true = JsonValue::parse("true");
    ASSERT_TRUE(val_true.is_bool());
    ASSERT_TRUE(val_true.as_bool());

    auto val_false = JsonValue::parse("false");
    ASSERT_TRUE(val_false.is_bool());
    ASSERT_FALSE(val_false.as_bool());

    auto val_int = JsonValue::parse("12345");
    ASSERT_TRUE(val_int.is_number());
    ASSERT_EQ(val_int.as_number(), 12345.0);

    auto val_neg = JsonValue::parse("-42.5");
    ASSERT_TRUE(val_neg.is_number());
    ASSERT_EQ(val_neg.as_number(), -42.5);

    auto val_str = JsonValue::parse("\"hello world\"");
    ASSERT_TRUE(val_str.is_string());
    ASSERT_EQ(val_str.as_string(), "hello world");

    auto val_esc = JsonValue::parse("\"line1\\nline2\\t\\\"quoted\\\"\"");
    ASSERT_TRUE(val_esc.is_string());
    ASSERT_EQ(val_esc.as_string(), "line1\nline2\t\"quoted\"");
}

TEST_CASE(Update, JsonParserComplex) {
    std::string json_str = R"({
        "tag_name": "v1.11.0",
        "name": "MiniGit Release 1.11.0",
        "draft": false,
        "prerelease": false,
        "assets_count": 3,
        "assets": [
            {
                "name": "minigit.exe",
                "size": 5242880,
                "browser_download_url": "https://github.com/sagarkrjha/minigit/releases/download/v1.11.0/minigit.exe"
            },
            {
                "name": "minigit-linux",
                "size": 6291456,
                "browser_download_url": "https://github.com/sagarkrjha/minigit/releases/download/v1.11.0/minigit-linux"
            }
        ]
    })";

    auto root = JsonValue::parse(json_str);
    ASSERT_TRUE(root.is_object());
    ASSERT_TRUE(root.contains("tag_name"));
    ASSERT_EQ(root.get_string("tag_name"), "v1.11.0");
    ASSERT_EQ(root.get_string("name"), "MiniGit Release 1.11.0");
    ASSERT_FALSE(root.get_bool("draft"));
    ASSERT_EQ(root.get_number("assets_count"), 3.0);

    const auto& assets = root["assets"];
    ASSERT_TRUE(assets.is_array());
    ASSERT_EQ(assets.as_array().size(), 2);

    const auto& asset0 = assets[0];
    ASSERT_TRUE(asset0.is_object());
    ASSERT_EQ(asset0.get_string("name"), "minigit.exe");
    ASSERT_EQ(asset0.get_number("size"), 5242880.0);
    ASSERT_EQ(asset0.get_string("browser_download_url"),
              "https://github.com/sagarkrjha/minigit/releases/download/v1.11.0/minigit.exe");

    const auto& asset1 = assets[1];
    ASSERT_TRUE(asset1.is_object());
    ASSERT_EQ(asset1.get_string("name"), "minigit-linux");
}

TEST_CASE(Update, ReleaseParsing) {
    std::string release_json = R"({
        "tag_name": "v1.11.0",
        "name": "MiniGit v1.11.0",
        "body": "Major improvements and new features",
        "html_url": "https://github.com/sagarkrjha/minigit/releases/tag/v1.11.0",
        "published_at": "2026-09-23T12:00:00Z",
        "assets": [
            {
                "name": "minigit.exe",
                "browser_download_url": "https://example.com/minigit.exe",
                "size": 1000
            },
            {
                "name": "minigit-linux",
                "browser_download_url": "https://example.com/minigit-linux",
                "size": 2000
            },
            {
                "name": "minigit-macos",
                "browser_download_url": "https://example.com/minigit-macos",
                "size": 3000
            }
        ]
    })";

    auto info = parse_release_json(release_json);
    ASSERT_TRUE(info.has_value());
    ASSERT_EQ(info->tag_name, "v1.11.0");
    ASSERT_EQ(info->title, "MiniGit v1.11.0");
    ASSERT_EQ(info->body, "Major improvements and new features");
    ASSERT_EQ(info->html_url, "https://github.com/sagarkrjha/minigit/releases/tag/v1.11.0");
    ASSERT_EQ(info->published_at, "2026-09-23T12:00:00Z");
    ASSERT_EQ(info->version.major, 1);
    ASSERT_EQ(info->version.minor, 11);
    ASSERT_EQ(info->version.patch, 0);
    ASSERT_EQ(info->assets.size(), 3);

    const auto* asset = info->find_platform_asset();
    ASSERT_TRUE(asset != nullptr);
    ASSERT_FALSE(asset->name.empty());
    ASSERT_FALSE(asset->download_url.empty());

    // Update availability
    auto current_v1_10 = SemVer::parse("1.10.0");
    ASSERT_TRUE(current_v1_10.has_value());
    ASSERT_TRUE(is_update_available(*info, *current_v1_10));

    auto current_v1_11 = SemVer::parse("1.11.0");
    ASSERT_TRUE(current_v1_11.has_value());
    ASSERT_FALSE(is_update_available(*info, *current_v1_11));

    auto current_v2_0 = SemVer::parse("2.0.0");
    ASSERT_TRUE(current_v2_0.has_value());
    ASSERT_FALSE(is_update_available(*info, *current_v2_0));
}

TEST_CASE(Update, OsDispatchUnorderedMap) {
    ASSERT_EQ(os_to_string(OperatingSystem::Windows), "windows");
    ASSERT_EQ(os_to_string(OperatingSystem::MacOS), "macos");
    ASSERT_EQ(os_to_string(OperatingSystem::Linux), "linux");
    ASSERT_EQ(os_to_string(OperatingSystem::Unknown), "unknown");

    std::string release_json = R"({
        "tag_name": "v1.11.0",
        "name": "MiniGit v1.11.0",
        "assets": [
            { "name": "minigit.exe", "browser_download_url": "https://example.com/minigit.exe", "size": 1000 },
            { "name": "minigit-linux", "browser_download_url": "https://example.com/minigit-linux", "size": 2000 },
            { "name": "minigit-macos", "browser_download_url": "https://example.com/minigit-macos", "size": 3000 }
        ]
    })";

    auto info = parse_release_json(release_json);
    ASSERT_TRUE(info.has_value());

    // Test Windows OS lookup via unordered_map
#if defined(_WIN32)
    _putenv("MINIGIT_OS_OVERRIDE=windows");
#else
    setenv("MINIGIT_OS_OVERRIDE", "windows", 1);
#endif
    ASSERT_EQ(get_current_os(), OperatingSystem::Windows);
    ASSERT_EQ(get_current_platform_asset_name(), "minigit.exe");
    const auto* win_asset = info->find_platform_asset();
    ASSERT_TRUE(win_asset != nullptr);
    ASSERT_EQ(win_asset->name, "minigit.exe");

    // Test Linux OS lookup via unordered_map
#if defined(_WIN32)
    _putenv("MINIGIT_OS_OVERRIDE=linux");
#else
    setenv("MINIGIT_OS_OVERRIDE", "linux", 1);
#endif
    ASSERT_EQ(get_current_os(), OperatingSystem::Linux);
    ASSERT_EQ(get_current_platform_asset_name(), "minigit-linux");
    const auto* linux_asset = info->find_platform_asset();
    ASSERT_TRUE(linux_asset != nullptr);
    ASSERT_EQ(linux_asset->name, "minigit-linux");

    // Test macOS OS lookup via unordered_map
#if defined(_WIN32)
    _putenv("MINIGIT_OS_OVERRIDE=macos");
#else
    setenv("MINIGIT_OS_OVERRIDE", "macos", 1);
#endif
    ASSERT_EQ(get_current_os(), OperatingSystem::MacOS);
    ASSERT_EQ(get_current_platform_asset_name(), "minigit-macos");
    const auto* mac_asset = info->find_platform_asset();
    ASSERT_TRUE(mac_asset != nullptr);
    ASSERT_EQ(mac_asset->name, "minigit-macos");

    // Reset override
#if defined(_WIN32)
    _putenv("MINIGIT_OS_OVERRIDE=");
#else
    unsetenv("MINIGIT_OS_OVERRIDE");
#endif
}

TEST_CASE(Update, ReplaceExecutable) {
    auto dir = make_temp_test_dir("replace_exe");
    auto target_file = dir / "my_app.bin";
    auto new_file = dir / "my_app.bin.new";

    // Write original binary content
    {
        std::ofstream out(target_file, std::ios::binary);
        out << "ORIGINAL_BINARY_CONTENT";
    }

    // Write new binary content
    {
        std::ofstream out(new_file, std::ios::binary);
        out << "NEW_UPDATED_BINARY_CONTENT";
    }

    bool success = replace_executable(target_file, new_file);
    ASSERT_TRUE(success);
    ASSERT_TRUE(fs::exists(target_file));

    // Verify content of target_file is updated
    std::ifstream in(target_file, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ASSERT_EQ(content, "NEW_UPDATED_BINARY_CONTENT");

    remove_test_dir(dir);
}

TEST_CASE(Update, NotifierCacheHandling) {
    auto dir = make_temp_test_dir("cache");
    auto cache_file = dir / "update_cache";

    UpdateNotifier& notifier = UpdateNotifier::instance();
    notifier.set_cache_path(cache_file.string());
    notifier.set_check_interval_seconds(3600);

    ASSERT_EQ(notifier.get_cache_path(), cache_file.string());
    ASSERT_EQ(notifier.get_check_interval_seconds(), 3600);

    // Pre-populate cache with a newer version and recent timestamp
    uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    {
        std::ofstream out(cache_file);
        out << "last_check=" << now << "\n"
            << "latest_version=v2.0.0\n"
            << "download_url=https://github.com/sagarkrjha/minigit/releases/tag/v2.0.0\n";
    }

    ASSERT_TRUE(fs::exists(cache_file));

    // Excluded commands should be skipped without throwing
    notifier.check_and_notify("version");
    notifier.check_and_notify("update");
    notifier.check_and_notify("hash-object");
    notifier.check_and_notify("cat-file");
    notifier.check_and_notify("write-tree");
    notifier.check_and_notify("ls-files");

    // Reset custom cache path
    notifier.set_cache_path("");
    remove_test_dir(dir);
}

TEST_CASE(Update, UpdateCommandHelp) {
    char const* argv[] = {"minigit", "update", "--help"};
    int res = update_command(3, argv);
    ASSERT_EQ(res, 0);

    char const* argv_short[] = {"minigit", "update", "-h"};
    int res_short = update_command(3, argv_short);
    ASSERT_EQ(res_short, 0);

    char const* argv_bad[] = {"minigit", "update", "--unknown-flag"};
    int res_bad = update_command(3, argv_bad);
    ASSERT_EQ(res_bad, 1);
}
