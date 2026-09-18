#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct SubmoduleEntry
{
    std::string name;    // Logical name (e.g. "libs/engine")
    std::string path;    // Path relative to repository root (e.g. "libs/engine")
    std::string url;     // Repository URL or filesystem path
    std::string branch;  // Optional branch name (e.g. "main")
};

class SubmoduleConfig
{
public:
    explicit SubmoduleConfig(const std::filesystem::path &file_path);

    // Save current submodule entries back to .minigitmodules
    void save() const;

    const std::vector<SubmoduleEntry> &entries() const { return entries_; }

    const SubmoduleEntry *find_by_path(const std::string &path) const;
    const SubmoduleEntry *find_by_name(const std::string &name) const;

    void add_or_update(const SubmoduleEntry &entry);
    bool remove(const std::string &path_or_name);

    // Static helper to check if a relative path in a repository is a registered submodule or gitlink
    static bool is_submodule_path(const std::filesystem::path &repo_root, const std::string &rel_path);

    // Static helpers for .minigit/config submodule registration
    static void set_config_entry(const std::filesystem::path &git_dir,
                                 const std::string &name,
                                 const std::string &url,
                                 bool active = true);
    static std::optional<std::string> get_config_url(const std::filesystem::path &git_dir,
                                                     const std::string &name);
    static bool is_config_active(const std::filesystem::path &git_dir,
                                 const std::string &name);
    static void remove_config_entry(const std::filesystem::path &git_dir,
                                    const std::string &name);

private:
    std::filesystem::path file_path_;
    std::vector<SubmoduleEntry> entries_;

    void load();
};
