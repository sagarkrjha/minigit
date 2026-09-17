#pragma once

#include <filesystem>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Result of a rebase operation.
// ---------------------------------------------------------------------------
struct RebaseResult
{
    bool success{false};
    bool conflict{false};
    bool up_to_date{false};
    bool fast_forward{false};
    bool aborted{false};
    std::string new_head_sha;
    std::vector<std::string> conflicted_files;
    std::string error_message;
};

// ---------------------------------------------------------------------------
// Programmatic APIs
// ---------------------------------------------------------------------------

// Replay commits on the current branch (or HEAD) onto `upstream` (or `onto`).
// If `onto` is empty, commits are replayed directly onto `upstream`.
RebaseResult perform_rebase(
    const std::filesystem::path& repo_root,
    const std::string& upstream,
    const std::string& onto = ""
);

// Continue an in-progress rebase after resolving merge conflicts.
RebaseResult rebase_continue(const std::filesystem::path& repo_root);

// Abort an in-progress rebase and restore original branch and HEAD state.
RebaseResult rebase_abort(const std::filesystem::path& repo_root);

// Skip the current conflicted commit and continue replaying remaining commits.
RebaseResult rebase_skip(const std::filesystem::path& repo_root);

// ---------------------------------------------------------------------------
// CLI entry point:
// minigit rebase [-i] [--onto <newbase>] <upstream>
// minigit rebase --continue
// minigit rebase --abort
// minigit rebase --skip
// ---------------------------------------------------------------------------
int rebase_command(int argc, char const* argv[]);
