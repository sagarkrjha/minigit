#pragma once

#include <filesystem>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Options for minigit ls-files.
// ---------------------------------------------------------------------------
struct LsFilesOptions
{
    bool stage{false};              // -s, --stage: show mode, SHA-256, stage number
    bool cached{false};             // -c, --cached: show cached/tracked files
    bool deleted{false};            // -d, --deleted: show files deleted from working tree
    bool modified{false};           // -m, --modified: show files modified in working tree
    bool others{false};             // -o, --others: show untracked files
    std::vector<std::string> paths; // optional path filters
};

// ---------------------------------------------------------------------------
// Result of ls-files query.
// ---------------------------------------------------------------------------
struct LsFilesResult
{
    bool success{false};
    std::vector<std::string> entries;
    std::string output;
    std::string error_message;
};

// ---------------------------------------------------------------------------
// Core programmatic ls-files API.
// ---------------------------------------------------------------------------
LsFilesResult perform_ls_files(
    const std::filesystem::path& repo_root,
    const LsFilesOptions& options
);

// ---------------------------------------------------------------------------
// CLI entry point: minigit ls-files [-s] [-c] [-d] [-m] [-o] [<path>...]
// ---------------------------------------------------------------------------
int ls_files_command(int argc, char const *argv[]);
