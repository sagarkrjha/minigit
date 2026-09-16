#pragma once

#include <filesystem>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Result of a cherry-pick operation.
// ---------------------------------------------------------------------------
struct CherryPickResult
{
    bool success{false};
    bool conflict{false};
    std::string new_commit_sha;
    std::vector<std::string> conflicted_files;
    std::string error_message;
};

// ---------------------------------------------------------------------------
// Core programmatic cherry-pick API.
//
// Applies the changeset introduced by `target` (commit SHA, branch, or tag)
// onto the current HEAD of the repository at `repo_root`.
//
// Parameters:
//   repo_root:    Root directory of the MiniGit repository.
//   target:       Commit SHA (full or short), branch name, or tag name.
//   author:       Author override; if empty, preserves original commit author.
//   no_commit:    If true, updates working directory and index without creating
//                 a commit object or advancing refs.
//   parent_index: For merge commits, 1-based index specifying which parent is
//                 considered the mainline base (defaults to 1).
// ---------------------------------------------------------------------------
CherryPickResult perform_cherry_pick(
    const std::filesystem::path& repo_root,
    const std::string& target,
    const std::string& author = "",
    bool no_commit = false,
    int parent_index = 1
);

// ---------------------------------------------------------------------------
// CLI entry point: minigit cherry-pick [-n] [--author <author>] [-m <parent>] <commit>
//
// Discovers repository from the current working directory, executes the
// cherry-pick, prints standard git messages, and exits with code 1 on conflict
// or failure.
// ---------------------------------------------------------------------------
void cherry_pick_command(
    const std::string& target,
    const std::string& author = "",
    bool no_commit = false,
    int parent_index = 1
);
