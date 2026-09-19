#pragma once

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Line-level LCS-based diff engine.
// Produces unified-diff-style output.
// ---------------------------------------------------------------------------

enum class EditType
{
    Keep,    // line present in both old and new
    Add,     // line only in new
    Remove,  // line only in old
};

struct Edit
{
    EditType type;
    std::string line;
};

// Split `text` into lines (strips trailing \r from each line).
std::vector<std::string> split_lines(const std::string &text);

// Compute the edit sequence (Keep/Add/Remove) between `old_lines` and
// `new_lines` using Eugene Myers' O(ND) greedy difference algorithm.
std::vector<Edit> myers_diff(const std::vector<std::string> &old_lines,
                             const std::vector<std::string> &new_lines);

// Alias for backwards compatibility across callers
std::vector<Edit> lcs_diff(const std::vector<std::string> &old_lines,
                            const std::vector<std::string> &new_lines);

// Format `edits` as a unified diff with `context` surrounding lines.
// `old_name` / `new_name` are used in the --- / +++ header lines.
// Returns an empty string if there are no differences.
std::string format_unified_diff(const std::string &old_name,
                                const std::string &new_name,
                                const std::vector<Edit> &edits,
                                int context = 3);
