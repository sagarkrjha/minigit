#include "ignore.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Glob matching
// ---------------------------------------------------------------------------
// Matches `str` against `pattern` where:
//   '*'  matches any sequence of characters that does NOT include '/'
//   '?'  matches exactly one character that is NOT '/'
//   All other characters are literal.
//
// Uses a simple recursive backtracking approach; patterns in .minigitignore
// files are short so performance is not a concern.
// ---------------------------------------------------------------------------
bool IgnoreRules::glob_match(const std::string& pattern, const std::string& str)
{
    const char* p = pattern.c_str();
    const char* s = str.c_str();

    // Iterative DP using two pointers + a bookmark for '*' backtracking.
    const char* star_p  = nullptr; // position of last '*' in pattern
    const char* star_s  = nullptr; // position in str when '*' was matched

    while (*s != '\0')
    {
        if (*p == '*')
        {
            // '*' cannot match '/'; skip consecutive '*'s.
            while (*p == '*') ++p;

            if (*p == '\0')
            {
                // Trailing '*': match everything left (no '/' allowed).
                while (*s != '\0')
                {
                    if (*s == '/') return false;
                    ++s;
                }
                return true;
            }

            // Bookmark position for possible backtrack.
            star_p = p;
            star_s = s;
        }
        else if (*p == '?' && *s != '/')
        {
            // '?' matches exactly one non-'/' character.
            ++p;
            ++s;
        }
        else if (*p == *s)
        {
            ++p;
            ++s;
        }
        else if (star_p != nullptr && *star_s != '/')
        {
            // Backtrack: let '*' consume one more character.
            p = star_p;
            s = ++star_s;
        }
        else
        {
            return false;
        }
    }

    // Consume any trailing '*'s in the pattern.
    while (*p == '*') ++p;

    return *p == '\0';
}

// ---------------------------------------------------------------------------
// IgnoreRules::load
// ---------------------------------------------------------------------------
IgnoreRules IgnoreRules::load(const fs::path& repo_root)
{
    IgnoreRules rules;

    const fs::path ignore_file = repo_root / ".minigitignore";
    std::ifstream  f(ignore_file);
    if (!f)
        return rules; // file absent – nothing ignored

    std::string line;
    while (std::getline(f, line))
    {
        // Strip trailing '\r' (Windows line endings).
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Skip blank lines and comments.
        if (line.empty() || line.front() == '#')
            continue;

        Rule rule;
        std::string_view sv(line);

        // Leading '!' → negate rule.
        if (sv.front() == '!')
        {
            rule.negate = true;
            sv.remove_prefix(1);
        }
        else
        {
            rule.negate = false;
        }

        // Trailing '/' → directory-only match; strip it from the pattern.
        if (!sv.empty() && sv.back() == '/')
        {
            rule.dir_only = true;
            sv.remove_suffix(1);
        }
        else
        {
            rule.dir_only = false;
        }

        if (sv.empty())
            continue; // degenerate pattern, skip

        rule.pattern = std::string(sv);

        // A pattern is "rooted" (matched against the full relative path) if it
        // contains a '/' *after* stripping the leading '!'.
        rule.rooted = (rule.pattern.find('/') != std::string::npos);

        rules.rules_.push_back(std::move(rule));
    }

    return rules;
}

// ---------------------------------------------------------------------------
// IgnoreRules::is_ignored
// ---------------------------------------------------------------------------
bool IgnoreRules::is_ignored(const std::string& rel_path) const
{
    if (rules_.empty())
        return false;

    // Derive the basename (filename component) of rel_path.
    const fs::path fsp(rel_path);
    const std::string basename = fsp.filename().string();

    bool ignored = false;

    for (const auto& rule : rules_)
    {
        bool matched = false;

        if (rule.rooted)
        {
            // Match against the full relative path.
            matched = glob_match(rule.pattern, rel_path);
        }
        else
        {
            // Non-rooted patterns match the basename first.
            matched = glob_match(rule.pattern, basename);

            if (!matched)
            {
                // For dir_only patterns (trailing '/'), walk every ancestor
                // directory component to see if any directory matches.
                // For non-dir-only patterns, git also checks path components
                // so that e.g. "build" matches "build/foo.o".
                fs::path cur = fsp;
                cur = cur.parent_path(); // strip filename; iterate directories
                while (!cur.empty() && cur != cur.root_path())
                {
                    const std::string dir_name = cur.filename().string();
                    if (dir_name.empty()) break;

                    if (glob_match(rule.pattern, dir_name))
                    {
                        // dir_only rules only match when the pattern names a dir.
                        // Non-dir-only rules always propagate to children.
                        if (!rule.dir_only || true)
                            matched = true;
                        break;
                    }
                    cur = cur.parent_path();
                }
            }
        }

        if (matched)
            ignored = !rule.negate; // negate flips the current state
    }

    return ignored;
}
