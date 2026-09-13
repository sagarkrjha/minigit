#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Holds parsed rules from a .minigitignore file and answers is_ignored queries.
class IgnoreRules
{
public:
    // Load rules from <repo_root>/.minigitignore.
    // If the file does not exist the object is empty (nothing is ignored).
    static IgnoreRules load(const std::filesystem::path& repo_root);

    // Returns true if the given repo-relative path (forward-slash separated)
    // should be ignored according to the loaded rules.
    bool is_ignored(const std::string& rel_path) const;

private:
    // A single parsed rule.
    struct Rule
    {
        std::string pattern;  // raw pattern (after stripping leading '!')
        bool        negate;   // true  → this is an un-ignore rule
        bool        dir_only; // true  → pattern ends with '/'
        bool        rooted;   // true  → pattern contains '/' (match full path)
    };

    std::vector<Rule> rules_;

    // Glob-match `str` against `pattern`.
    // '*' matches any run of non-'/' chars; '?' matches exactly one non-'/' char.
    static bool glob_match(const std::string& pattern, const std::string& str);
};
