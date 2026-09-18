#pragma once

#include <filesystem>
#include <string>

namespace minigit::core {

// Validates that a relative path is well-formed, does not attempt directory traversal (e.g. "../"),
// and does not target protected metadata directories (.minigit or .git).
bool is_safe_repo_relpath(const std::string &rel_path);

// Resolves a relative path against the repository root after verifying it does not escape.
// Throws std::runtime_error if the path is invalid or attempts traversal.
std::filesystem::path resolve_safe_repo_path(
    const std::filesystem::path &root,
    const std::string &rel_path
);

} // namespace minigit::core
