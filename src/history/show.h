#pragma once

#include <filesystem>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Format modes for minigit show.
// ---------------------------------------------------------------------------
enum class ShowFormat
{
    Default,   // Metadata + full unified diff
    Stat,      // Metadata + diffstat summary
    NameOnly   // Metadata + modified filenames only
};

// ---------------------------------------------------------------------------
// Result of inspecting an object.
// ---------------------------------------------------------------------------
struct ShowResult
{
    bool success{false};
    std::string object_sha;
    std::string object_type; // "commit", "tree", "blob", "tag"
    std::string output;
    std::string error_message;
};

// ---------------------------------------------------------------------------
// Core programmatic show API.
//
// Resolves target (HEAD, branch, tag, full SHA, short SHA, or ancestry suffix
// like ~N and ^) and formats the object inspection output.
// ---------------------------------------------------------------------------
ShowResult perform_show(
    const std::filesystem::path& repo_root,
    const std::string& target = "HEAD",
    ShowFormat format = ShowFormat::Default
);

// ---------------------------------------------------------------------------
// CLI entry point: minigit show [--stat | --name-only] [<object>]
// ---------------------------------------------------------------------------
int show_command(int argc, char const *argv[]);
