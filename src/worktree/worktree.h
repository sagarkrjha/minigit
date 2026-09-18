#pragma once

#include "repository/repository.h"

#include <filesystem>
#include <string>
#include <vector>

namespace minigit::worktree {

struct WorktreeInfo
{
    std::string id;
    std::filesystem::path path;
    std::filesystem::path git_dir;
    std::string head_sha;
    std::string branch;          // e.g. "refs/heads/main" or "main"
    bool is_main{false};
    bool is_detached{false};
    bool is_locked{false};
    std::string lock_reason;
    bool is_prunable{false};
    std::string prunable_reason;
};

// Discover all worktrees (main worktree + all linked worktrees in .minigit/worktrees/)
std::vector<WorktreeInfo> get_all_worktrees(const Repository& repo);

// Subcommand handlers
int worktree_add(int argc, char const* argv[]);
int worktree_list(int argc, char const* argv[]);
int worktree_remove(int argc, char const* argv[]);
int worktree_prune(int argc, char const* argv[]);
int worktree_lock(int argc, char const* argv[]);
int worktree_unlock(int argc, char const* argv[]);
int worktree_move(int argc, char const* argv[]);

// Main worktree dispatcher
int worktree_command(int argc, char const* argv[]);

} // namespace minigit::worktree
