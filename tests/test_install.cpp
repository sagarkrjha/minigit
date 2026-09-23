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

    std::string exe_name = "minigit";
#if defined(_WIN32)
    exe_name += ".exe";
#endif

    // Create fake source executable
    auto src_exe = src_dir / exe_name;
    {
        std::ofstream out(src_exe, std::ios::binary);
        out << "MOCK_MINIGIT_BINARY_v1.11.0";
    }

    set_env("MINIGIT_MOCK_ENV_PATH", "C:\\existing");
    set_env("MINIGIT_MOCK_REGISTRY", "1");

    InstallOptions options;
    options.scope = InstallScope::User;
    options.custom_dir = target_dir;
    options.add_to_path = true;

    auto res = perform_install(options, src_exe);
    ASSERT_TRUE(res.success);
    ASSERT_TRUE(fs::exists(res.installed_cmd_exe));
    ASSERT_TRUE(fs::exists(res.installed_bin_exe));
    ASSERT_TRUE(fs::exists(res.installed_exe));
    ASSERT_EQ(res.installed_exe, res.installed_cmd_exe);
    ASSERT_EQ(res.installed_cmd_exe, target_dir / "cmd" / exe_name);
    ASSERT_EQ(res.installed_bin_exe, target_dir / "bin" / exe_name);
    ASSERT_TRUE(fs::exists(target_dir / "etc" / "minigitconfig"));
    ASSERT_TRUE(fs::exists(target_dir / "etc" / "templates"));
    ASSERT_TRUE(res.path_modified);
    ASSERT_TRUE(is_directory_in_env_path(target_dir / "cmd", InstallScope::User));
    ASSERT_TRUE(res.uninstall_registered);
    ASSERT_TRUE(res.context_menu_registered);

    // Verify content of installed files
    {
        std::ifstream in(res.installed_cmd_exe, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ASSERT_EQ(content, "MOCK_MINIGIT_BINARY_v1.11.0");
    }
    {
        std::ifstream in(res.installed_bin_exe, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ASSERT_EQ(content, "MOCK_MINIGIT_BINARY_v1.11.0");
    }

    // Reinstalling the exact same binary without --force is recognized as up to date
    auto res_repeat = perform_install(options, res.installed_cmd_exe);
    ASSERT_TRUE(res_repeat.success);
    ASSERT_TRUE(res_repeat.message.find("already installed") != std::string::npos);

    unset_env("MINIGIT_MOCK_ENV_PATH");
    unset_env("MINIGIT_MOCK_REGISTRY");
    remove_install_test_dir(temp_dir);
}

TEST_CASE(Install, PerformInstallForce) {
    auto temp_dir = make_temp_install_test_dir("force_inst");
    auto src_dir = temp_dir / "src";
    auto target_dir = temp_dir / "target";
    fs::create_directories(src_dir);
    fs::create_directories(target_dir / "cmd");
    fs::create_directories(target_dir / "bin");

    std::string exe_name = "minigit";
#if defined(_WIN32)
    exe_name += ".exe";
#endif

    auto src_exe = src_dir / exe_name;
    {
        std::ofstream out(src_exe, std::ios::binary);
        out << "NEW_BINARY_CONTENT";
    }

    auto target_cmd_exe = target_dir / "cmd" / exe_name;
    auto target_bin_exe = target_dir / "bin" / exe_name;
    {
        std::ofstream out(target_cmd_exe, std::ios::binary);
        out << "OLD_BINARY_CONTENT";
    }
    {
        std::ofstream out(target_bin_exe, std::ios::binary);
        out << "OLD_BINARY_CONTENT";
    }

    set_env("MINIGIT_MOCK_REGISTRY", "1");

    InstallOptions options;
    options.scope = InstallScope::User;
    options.custom_dir = target_dir;
    options.add_to_path = false;
    options.force = true;

    auto res = perform_install(options, src_exe);
    ASSERT_TRUE(res.success);

    // Verify target binaries were overwritten
    {
        std::ifstream in(target_cmd_exe, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ASSERT_EQ(content, "NEW_BINARY_CONTENT");
    }
    {
        std::ifstream in(target_bin_exe, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ASSERT_EQ(content, "NEW_BINARY_CONTENT");
    }

    unset_env("MINIGIT_MOCK_REGISTRY");
    remove_install_test_dir(temp_dir);
}

TEST_CASE(Install, PerformUninstall) {
    auto temp_dir = make_temp_install_test_dir("uninst");
    auto target_dir = temp_dir / "target";
    fs::create_directories(target_dir / "cmd");
    fs::create_directories(target_dir / "bin");
    fs::create_directories(target_dir / "etc");

    std::string exe_name = "minigit";
#if defined(_WIN32)
    exe_name += ".exe";
#endif
    auto target_cmd_exe = target_dir / "cmd" / exe_name;
    auto target_bin_exe = target_dir / "bin" / exe_name;
    {
        std::ofstream out(target_cmd_exe, std::ios::binary);
        out << "SAMPLE_CMD_BINARY";
    }
    {
        std::ofstream out(target_bin_exe, std::ios::binary);
        out << "SAMPLE_BIN_BINARY";
    }

    set_env("MINIGIT_MOCK_ENV_PATH", "C:\\bin;" + (target_dir / "cmd").string());
    set_env("MINIGIT_MOCK_REGISTRY", "1");
    register_windows_uninstall(target_dir, target_cmd_exe, InstallScope::User);
    register_explorer_context_menu(target_cmd_exe, InstallScope::User);

    ASSERT_TRUE(is_directory_in_env_path(target_dir / "cmd", InstallScope::User));
    ASSERT_TRUE(is_windows_uninstall_registered(InstallScope::User));
    ASSERT_TRUE(is_explorer_context_menu_registered(InstallScope::User));

    InstallOptions options;
    options.scope = InstallScope::User;
    options.custom_dir = target_dir;
    options.add_to_path = true;
    options.uninstall = true;

    auto res = perform_uninstall(options);
    ASSERT_TRUE(res.success);
    ASSERT_FALSE(fs::exists(target_cmd_exe));
    ASSERT_FALSE(fs::exists(target_bin_exe));
    ASSERT_TRUE(res.path_modified);
    ASSERT_FALSE(is_directory_in_env_path(target_dir / "cmd", InstallScope::User));
    ASSERT_FALSE(is_windows_uninstall_registered(InstallScope::User));
    ASSERT_FALSE(is_explorer_context_menu_registered(InstallScope::User));

    unset_env("MINIGIT_MOCK_ENV_PATH");
    unset_env("MINIGIT_MOCK_REGISTRY");
    remove_install_test_dir(temp_dir);
}

TEST_CASE(Install, WindowsRegistryIntegration) {
    set_env("MINIGIT_MOCK_REGISTRY", "1");

    auto temp_dir = make_temp_install_test_dir("reg");
    auto cmd_exe = temp_dir / "cmd" / "minigit.exe";

    ASSERT_FALSE(is_windows_uninstall_registered(InstallScope::User));
    ASSERT_TRUE(register_windows_uninstall(temp_dir, cmd_exe, InstallScope::User));
    ASSERT_TRUE(is_windows_uninstall_registered(InstallScope::User));
    ASSERT_TRUE(unregister_windows_uninstall(InstallScope::User));
    ASSERT_FALSE(is_windows_uninstall_registered(InstallScope::User));

    ASSERT_FALSE(is_explorer_context_menu_registered(InstallScope::User));
    ASSERT_TRUE(register_explorer_context_menu(cmd_exe, InstallScope::User));
    ASSERT_TRUE(is_explorer_context_menu_registered(InstallScope::User));
    ASSERT_TRUE(unregister_explorer_context_menu(InstallScope::User));
    ASSERT_FALSE(is_explorer_context_menu_registered(InstallScope::User));

    unset_env("MINIGIT_MOCK_REGISTRY");
    remove_install_test_dir(temp_dir);
}

TEST_CASE(Install, SetupSystemConfig) {
    auto temp_dir = make_temp_install_test_dir("syscfg");
    ASSERT_TRUE(setup_system_config(temp_dir));

    auto cfg_file = temp_dir / "etc" / "minigitconfig";
    ASSERT_TRUE(fs::exists(cfg_file));
    ASSERT_TRUE(fs::exists(temp_dir / "etc" / "templates"));

    std::ifstream in(cfg_file);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ASSERT_TRUE(content.find("autocrlf = true") != std::string::npos);

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
    std::string exe_name = "minigit";
#if defined(_WIN32)
    exe_name += ".exe";
#endif
    auto dummy_exe = temp_dir / exe_name;
    {
        std::ofstream out(dummy_exe, std::ios::binary);
        out << "CLI_TEST_BINARY";
    }
    set_env("MINIGIT_EXEC_PATH_OVERRIDE", dummy_exe.string().c_str());
    set_env("MINIGIT_MOCK_REGISTRY", "1");

    auto target_dir = temp_dir / "installed";
    std::string target_dir_str = target_dir.string();

    char const* install_argv[] = {
        "minigit", "install", "--user", "--dir", target_dir_str.c_str(), "--no-path", "--no-context-menu"
    };
    ASSERT_EQ(install_command(7, install_argv), 0);

    // Verify binary exists in target cmd and bin
    ASSERT_TRUE(fs::exists(target_dir / "cmd" / exe_name));
    ASSERT_TRUE(fs::exists(target_dir / "bin" / exe_name));

    // Valid uninstall from mock dir
    char const* uninstall_argv[] = {
        "minigit", "install", "--uninstall", "--dir", target_dir_str.c_str(), "--no-path"
    };
    ASSERT_EQ(install_command(6, uninstall_argv), 0);
    ASSERT_FALSE(fs::exists(target_dir / "cmd" / exe_name));
    ASSERT_FALSE(fs::exists(target_dir / "bin" / exe_name));

    unset_env("MINIGIT_EXEC_PATH_OVERRIDE");
    unset_env("MINIGIT_MOCK_REGISTRY");
    remove_install_test_dir(temp_dir);
}
