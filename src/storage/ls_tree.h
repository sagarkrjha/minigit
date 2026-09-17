#pragma once

#include <filesystem>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Options for minigit ls-tree.
// ---------------------------------------------------------------------------
struct LsTreeOptions
{
    bool recursive{false};          // -r: recurse into sub-trees
    bool tree_only{false};          // -d: show only trees (directories)
    bool show_trees{false};         // -t: show trees even when recursing (-r)
    bool name_only{false};          // --name-only: list only filenames
    bool object_only{false};        // --object-only: list only object SHAs
    std::string tree_ish;           // tree-ish identifier (tree sha, commit sha, branch, tag, HEAD)
    std::vector<std::string> paths; // optional path filters
};

// ---------------------------------------------------------------------------
// Result entry of ls-tree query.
// ---------------------------------------------------------------------------
struct LsTreeEntry
{
    std::string mode;
    std::string type; // "tree" or "blob"
    std::string sha;
    std::string path;
};

struct LsTreeResult
{
    bool success{false};
    std::vector<LsTreeEntry> entries;
    std::string output;
    std::string error_message;
};

// ---------------------------------------------------------------------------
// Core programmatic ls-tree API.
// ---------------------------------------------------------------------------
LsTreeResult perform_ls_tree(
    const std::filesystem::path& repo_root,
    const LsTreeOptions& options
);

// ---------------------------------------------------------------------------
// CLI entry point: minigit ls-tree [-d] [-r] [-t] [--name-only] [--object-only] <tree-ish> [<path>...]
// ---------------------------------------------------------------------------
int ls_tree_command(int argc, char const *argv[]);
