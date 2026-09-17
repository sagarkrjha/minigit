#pragma once

#include <filesystem>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Options for the minigit clean command.
// ---------------------------------------------------------------------------
struct CleanOptions
{
    bool force{false};              // -f, --force: actually remove untracked files
    bool dry_run{false};            // -n, --dry-run: display what would be removed
    bool remove_directories{false}; // -d: remove entire untracked directories
    bool include_ignored{false};    // -x: also remove ignored files
    std::vector<std::string> paths; // optional path filters
};

// ---------------------------------------------------------------------------
// Result of a clean operation.
// ---------------------------------------------------------------------------
struct CleanResult
{
    bool success{false};
    std::vector<std::string> items_cleaned; // relative paths that were (or would be) removed
    std::string output;
    std::string error_message;
};

// ---------------------------------------------------------------------------
// Core programmatic clean API.
// ---------------------------------------------------------------------------
CleanResult perform_clean(
    const std::filesystem::path& repo_root,
    const CleanOptions& options
);

// ---------------------------------------------------------------------------
// CLI entry point: minigit clean [-f | -n] [-d] [-x] [<path>...]
// ---------------------------------------------------------------------------
int clean_command(int argc, char const *argv[]);
