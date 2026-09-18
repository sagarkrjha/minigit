#include "path_safety.h"

#include <algorithm>
#include <stdexcept>

namespace fs = std::filesystem;

namespace minigit::core {

bool is_safe_repo_relpath(const std::string &rel_path)
{
    if (rel_path.empty())
        return false;

    // Reject paths starting with / or \ or null bytes
    if (rel_path.front() == '/' || rel_path.front() == '\\' || rel_path.find('\0') != std::string::npos)
        return false;

    // Reject Windows drive letters (e.g. "C:...")
    if (rel_path.size() >= 2 && rel_path[1] == ':')
        return false;

    fs::path p(rel_path);

    // Must be a relative path
    if (p.is_absolute())
        return false;

    // Reject any component containing ".."
    for (const auto &part : p)
    {
        std::string s = part.string();
        if (s == "..")
            return false;
    }

    fs::path normal = p.lexically_normal();
    std::string gen = normal.generic_string();

    if (gen == "." || gen == ".." || gen.rfind("../", 0) == 0)
        return false;

    // Reject targeting .minigit or .git metadata directories
    if (gen == ".minigit" || gen.rfind(".minigit/", 0) == 0 ||
        gen == ".git" || gen.rfind(".git/", 0) == 0)
    {
        return false;
    }

    return true;
}

fs::path resolve_safe_repo_path(
    const fs::path &root,
    const std::string &rel_path)
{
    if (!is_safe_repo_relpath(rel_path))
    {
        throw std::runtime_error("Security violation: path traversal attempt detected in '" + rel_path + "'");
    }

    return (root / rel_path).lexically_normal();
}

} // namespace minigit::core
