#include "test_framework.h"
#include "install/install.h"
#include "core/version.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace minigit::install;

namespace {

fs::path make_temp_install_test_dir(const std::string& prefix) {
    auto p = fs::temp_directory_path() /
        ("minigit_install_test_" + prefix + "_" +
         std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    fs::create_directories(p);
    return p;
}

void remove_install_test_dir(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

void set_env(const std::string& key, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(key.c_str(), value.c_str());
#else
    setenv(key.c_str(), value.c_str(), 1);
#endif
}

void unset_env(const char* key) {
#if defined(_WIN32)
    _putenv_s(key, "");
#else
    unsetenv(key);
#endif
}

} // namespace

TEST_CASE(Install, ScopeParsingAndToString) {
    ASSERT_EQ(scope_to_string(InstallScope::User), "User");
    ASSERT_EQ(scope_to_string(InstallScope::System), "System");
    ASSERT_EQ(scope_to_string(InstallScope::Auto), "Auto");

    auto s1 = parse_scope("user");
    ASSERT_TRUE(s1.has_value());
    ASSERT_TRUE(*s1 == InstallScope::User);

    auto s2 = parse_scope("System");
    ASSERT_TRUE(s2.has_value());
    ASSERT_TRUE(*s2 == InstallScope::System);

    auto s3 = parse_scope("auto");
    ASSERT_TRUE(s3.has_value());
    ASSERT_TRUE(*s3 == InstallScope::Auto);

    auto s4 = parse_scope("invalid");
    ASSERT_FALSE(s4.has_value());
}

TEST_CASE(Install, AdminPrivilegeDetectionOverride) {
    set_env("MINIGIT_ADMIN_OVERRIDE", "1");
    ASSERT_TRUE(is_running_as_admin());

    set_env("MINIGIT_ADMIN_OVERRIDE", "0");
    ASSERT_FALSE(is_running_as_admin());

    set_env("MINIGIT_ADMIN_OVERRIDE", "true");
    ASSERT_TRUE(is_running_as_admin());

    set_env("MINIGIT_ADMIN_OVERRIDE", "false");
    ASSERT_FALSE(is_running_as_admin());

    unset_env("MINIGIT_ADMIN_OVERRIDE");
}

TEST_CASE(Install, WritePermissionDetection) {
    auto test_dir = make_temp_install_test_dir("perm");

    // Standard writable directory
    ASSERT_TRUE(can_write_to_directory(test_dir));

    // Non-existent subdirectory whose parent is writable
    auto sub_dir = test_dir / "subdir" / "deep";
    ASSERT_TRUE(can_write_to_directory(sub_dir));

    // Existing writable file
    auto probe_file = test_dir / "test.bin";
    {
        std::ofstream out(probe_file);
        out << "test";
    }
    ASSERT_TRUE(can_write_to_file(probe_file));

    // Non-existent file in writable directory
    auto non_existent = test_dir / "does_not_exist.bin";
    ASSERT_TRUE(can_write_to_file(non_existent));

    // Override tests
    set_env("MINIGIT_WRITE_PERM_OVERRIDE", "0");
    ASSERT_FALSE(can_write_to_directory(test_dir));
    ASSERT_FALSE(can_write_to_file(probe_file));

    set_env("MINIGIT_WRITE_PERM_OVERRIDE", "1");
    ASSERT_TRUE(can_write_to_directory(test_dir));
    ASSERT_TRUE(can_write_to_file(probe_file));

    unset_env("MINIGIT_WRITE_PERM_OVERRIDE");
    remove_install_test_dir(test_dir);
}

TEST_CASE(Install, PathListManipulation) {
    char sep = ';';
    std::string path_list = "C:\\Windows\\system32;C:\\Program Files\\app;D:\\tools\\bin";

    ASSERT_TRUE(is_in_path_list(path_list, "C:\\Program Files\\app", sep));
    ASSERT_TRUE(is_in_path_list(path_list, "c:/program files/app", sep)); // normalized check
    ASSERT_TRUE(is_in_path_list(path_list, "C:\\Windows\\system32", sep));
    ASSERT_TRUE(is_in_path_list(path_list, "D:\\tools\\bin", sep));
    ASSERT_FALSE(is_in_path_list(path_list, "C:\\Program Files\\missing", sep));

    // Add directory
    std::string updated = add_to_path_list(path_list, "C:\\minigit\\bin", sep);
    ASSERT_TRUE(is_in_path_list(updated, "C:\\minigit\\bin", sep));
    ASSERT_TRUE(is_in_path_list(updated, "C:\\Windows\\system32", sep));

    // Adding already present directory does not duplicate
    std::string unchanged = add_to_path_list(updated, "C:\\minigit\\bin", sep);
    ASSERT_EQ(updated, unchanged);

    // Remove directory
    std::string removed = remove_from_path_list(updated, "C:\\Program Files\\app", sep);
    ASSERT_FALSE(is_in_path_list(removed, "C:\\Program Files\\app", sep));
    ASSERT_TRUE(is_in_path_list(removed, "C:\\minigit\\bin", sep));
    ASSERT_TRUE(is_in_path_list(removed, "C:\\Windows\\system32", sep));

    // Empty list operations
    std::string from_empty = add_to_path_list("", "C:\\bin", sep);
    ASSERT_EQ(from_empty, "C:\\bin");
    ASSERT_TRUE(is_in_path_list(from_empty, "C:\\bin", sep));

    std::string remove_only = remove_from_path_list("C:\\bin", "C:\\bin", sep);
    ASSERT_EQ(remove_only, "");
}

TEST_CASE(Install, EnvironmentPathMocking) {
    set_env("MINIGIT_MOCK_ENV_PATH", "C:\\toolA;C:\\toolB");

    ASSERT_TRUE(is_directory_in_env_path("C:\\toolA", InstallScope::User));
    ASSERT_TRUE(is_directory_in_env_path("C:\\toolB", InstallScope::System));
    ASSERT_FALSE(is_directory_in_env_path("C:\\minigit\\bin", InstallScope::User));

    bool add_ok = add_directory_to_env_path("C:\\minigit\\bin", InstallScope::User);
    ASSERT_TRUE(add_ok);
    ASSERT_TRUE(is_directory_in_env_path("C:\\minigit\\bin", InstallScope::User));

    bool rem_ok = remove_directory_from_env_path("C:\\toolA", InstallScope::User);
    ASSERT_TRUE(rem_ok);
    ASSERT_FALSE(is_directory_in_env_path("C:\\toolA", InstallScope::User));
    ASSERT_TRUE(is_directory_in_env_path("C:\\minigit\\bin", InstallScope::User));

    unset_env("MINIGIT_MOCK_ENV_PATH");
}

TEST_CASE(Install, PerformInstallUserScope) {
    auto temp_dir = make_temp_install_test_dir("user_inst");
    auto src_dir = temp_dir / "src";
    auto target_dir = temp_dir / "target";
    fs::create_directories(src_dir);

    // Create fake source executable
    auto src_exe = src_dir / "minigit.exe";
    {
        std::ofstream out(src_exe, std::ios::binary);
        out << "MOCK_MINIGIT_BINARY_v1.11.0";
    }

    set_env("MINIGIT_MOCK_ENV_PATH", "C:\\existing");

    InstallOptions options;
    options.scope = InstallScope::User;
    options.custom_dir = target_dir;
    options.add_to_path = true;

    auto res = perform_install(options, src_exe);
    ASSERT_TRUE(res.success);
    ASSERT_TRUE(fs::exists(res.installed_exe));
    ASSERT_TRUE(res.path_modified);
    ASSERT_TRUE(is_directory_in_env_path(target_dir, InstallScope::User));

    // Verify content of installed file
    {
        std::ifstream in(res.installed_exe, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ASSERT_EQ(content, "MOCK_MINIGIT_BINARY_v1.11.0");
    }

    // Reinstalling the exact same binary without --force is recognized as up to date
    auto res_repeat = perform_install(options, res.installed_exe);
    ASSERT_TRUE(res_repeat.success);
    ASSERT_TRUE(res_repeat.message.find("already installed") != std::string::npos);

    unset_env("MINIGIT_MOCK_ENV_PATH");
    remove_install_test_dir(temp_dir);
}

TEST_CASE(Install, PerformInstallForce) {
    auto temp_dir = make_temp_install_test_dir("force_inst");
    auto src_dir = temp_dir / "src";
    auto target_dir = temp_dir / "target";
    fs::create_directories(src_dir);
    fs::create_directories(target_dir);

    auto src_exe = src_dir / "minigit.exe";
    {
        std::ofstream out(src_exe, std::ios::binary);
        out << "NEW_BINARY_CONTENT";
    }

    auto target_exe = target_dir / "minigit.exe";
    {
        std::ofstream out(target_exe, std::ios::binary);
        out << "OLD_BINARY_CONTENT";
    }

    InstallOptions options;
    options.scope = InstallScope::User;
    options.custom_dir = target_dir;
    options.add_to_path = false;
    options.force = true;

    auto res = perform_install(options, src_exe);
    ASSERT_TRUE(res.success);

    // Verify target binary was overwritten
    {
        std::ifstream in(target_exe, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ASSERT_EQ(content, "NEW_BINARY_CONTENT");
    }

    remove_install_test_dir(temp_dir);
}

TEST_CASE(Install, PerformUninstall) {
    auto temp_dir = make_temp_install_test_dir("uninst");
    auto target_dir = temp_dir / "target";
    fs::create_directories(target_dir);

    std::string exe_name = "minigit";
#if defined(_WIN32)
    exe_name += ".exe";
#endif
    auto target_exe = target_dir / exe_name;
    {
        std::ofstream out(target_exe, std::ios::binary);
        out << "SAMPLE_BINARY";
    }

    set_env("MINIGIT_MOCK_ENV_PATH", "C:\\bin;" + target_dir.string());
    ASSERT_TRUE(is_directory_in_env_path(target_dir, InstallScope::User));

    InstallOptions options;
    options.scope = InstallScope::User;
    options.custom_dir = target_dir;
    options.add_to_path = true;
    options.uninstall = true;

    auto res = perform_uninstall(options);
    ASSERT_TRUE(res.success);
    ASSERT_FALSE(fs::exists(target_exe));
    ASSERT_TRUE(res.path_modified);
    ASSERT_FALSE(is_directory_in_env_path(target_dir, InstallScope::User));

    unset_env("MINIGIT_MOCK_ENV_PATH");
    remove_install_test_dir(temp_dir);
}

TEST_CASE(Install, PermissionDeniedFailsGracefully) {
    auto temp_dir = make_temp_install_test_dir("perm_denied");
    auto src_dir = temp_dir / "src";
    fs::create_directories(src_dir);

    auto src_exe = src_dir / "minigit.exe";
    {
        std::ofstream out(src_exe, std::ios::binary);
        out << "SAMPLE_BINARY";
    }

    set_env("MINIGIT_WRITE_PERM_OVERRIDE", "0");
    set_env("MINIGIT_SKIP_ELEVATION", "1");

    InstallOptions options;
    options.scope = InstallScope::System;
    options.custom_dir = temp_dir / "protected";
    options.add_to_path = false;

    auto res = perform_install(options, src_exe);
    ASSERT_FALSE(res.success);
    ASSERT_TRUE(res.message.find("Administrator") != std::string::npos ||
                res.message.find("Permission denied") != std::string::npos);

    unset_env("MINIGIT_WRITE_PERM_OVERRIDE");
    unset_env("MINIGIT_SKIP_ELEVATION");
    remove_install_test_dir(temp_dir);
}

TEST_CASE(Install, CommandLineInterface) {
    // --help
    char const* help_argv[] = {"minigit", "install", "--help"};
    ASSERT_EQ(install_command(3, help_argv), 0);

    // -h
    char const* h_argv[] = {"minigit", "install", "-h"};
    ASSERT_EQ(install_command(3, h_argv), 0);

    // Invalid argument
    char const* invalid_argv[] = {"minigit", "install", "--unknown-flag"};
    ASSERT_EQ(install_command(3, invalid_argv), 1);

    // Missing path for --dir
    char const* missing_dir_argv[] = {"minigit", "install", "--dir"};
    ASSERT_EQ(install_command(3, missing_dir_argv), 1);

    // Valid install to mock dir
    auto temp_dir = make_temp_install_test_dir("cli_inst");
    auto dummy_exe = temp_dir / "minigit.exe";
    {
        std::ofstream out(dummy_exe, std::ios::binary);
        out << "CLI_TEST_BINARY";
    }
    set_env("MINIGIT_EXEC_PATH_OVERRIDE", dummy_exe.string().c_str());

    auto target_dir = temp_dir / "installed";
    std::string target_dir_str = target_dir.string();

    char const* install_argv[] = {
        "minigit", "install", "--user", "--dir", target_dir_str.c_str(), "--no-path"
    };
    ASSERT_EQ(install_command(6, install_argv), 0);

    // Verify binary exists in target
    ASSERT_TRUE(fs::exists(target_dir / "minigit.exe"));

    // Valid uninstall from mock dir
    char const* uninstall_argv[] = {
        "minigit", "install", "--uninstall", "--dir", target_dir_str.c_str(), "--no-path"
    };
    ASSERT_EQ(install_command(6, uninstall_argv), 0);
    ASSERT_FALSE(fs::exists(target_dir / "minigit.exe"));

    unset_env("MINIGIT_EXEC_PATH_OVERRIDE");
    remove_install_test_dir(temp_dir);
}
