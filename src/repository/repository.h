#pragma once

#include <filesystem>
#include <string>

class Repository
{
public:
    explicit Repository(const std::filesystem::path& root);
    Repository(const std::filesystem::path& root,
               const std::filesystem::path& git_dir,
               const std::filesystem::path& common_dir,
               bool is_worktree = false,
               const std::string& worktree_name = "");

    // Walk parent directories from `start` upward until a .minigit directory
    // or .minigit gitdir file is found. Throws std::runtime_error if none is found.
    static Repository discover(const std::filesystem::path& start);

    const std::filesystem::path& root() const;
    const std::filesystem::path& git_dir() const;
    const std::filesystem::path& common_dir() const;

    bool is_worktree() const;
    const std::string& worktree_name() const;

    std::filesystem::path objects_dir() const;
    std::filesystem::path refs_dir() const;
    std::filesystem::path index_path() const;
    std::filesystem::path head_path() const;

    struct BranchWorktreeMatch
    {
        bool is_checked_out{false};
        std::filesystem::path worktree_path;
        bool is_current_worktree{false};
    };

    BranchWorktreeMatch find_branch_worktree(const std::string& branch_name) const;

    std::filesystem::path resolve_git_path(const std::filesystem::path& subpath) const;
    static std::filesystem::path resolve_path(const std::filesystem::path& git_dir,
                                              const std::filesystem::path& subpath);
    static std::string resolve_head_from_dir(const std::filesystem::path& git_dir);

    void init();

private:
    std::filesystem::path root_;
    std::filesystem::path git_dir_;
    std::filesystem::path common_dir_;
    bool is_worktree_{false};
    std::string worktree_name_;
};