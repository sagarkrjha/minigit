#include "status.h"

#include "core/sha256.h"
#include "ignore.h"
#include "index.h"
#include "storage/blob.h"
#include "storage/object_database.h"
#include "repository/repository.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>

namespace fs = std::filesystem;

// Read a file's bytes and return the blob SHA-256 that would be produced.
static std::string hash_file(const fs::path &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return {};

    // Use braces to avoid the "vexing parse" with istreambuf_iterator
    std::string content{
        std::istreambuf_iterator<char>{f},
        std::istreambuf_iterator<char>{}};

    Blob blob(std::move(content));
    return blob.id();
}


// Recursively collect all regular files under `root`, relative to `root`.
// Skips the .minigit directory and any path matched by ignore_rules.
// .minigitignore itself is also excluded from untracked output (it is never
// shown as untracked, but is still visible if it's already tracked).
static std::set<std::string> working_tree_files(const fs::path& root,
                                                const IgnoreRules& ignore_rules)
{
    std::set<std::string> result;

    for (const auto& entry : fs::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file())
            continue;

        const auto rel = fs::relative(entry.path(), root);
        const std::string rel_str = rel.generic_string();

        // Skip .minigit internals and any real .git directory.
        if (rel_str.starts_with(".minigit") || rel_str.starts_with(".git"))
            continue;

        // Never show .minigitignore as untracked (it is a config file).
        if (rel_str == ".minigitignore")
            continue;

        // Skip files matched by .minigitignore rules.
        if (ignore_rules.is_ignored(rel_str))
            continue;

        result.insert(rel_str);
    }

    return result;
}

void status()
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    Index index(repo.git_dir() / "index");
    const auto& staged = index.entries(); // path -> blob_id

    const IgnoreRules ignore_rules = IgnoreRules::load(repo.root());
    const auto wt_files = working_tree_files(repo.root(), ignore_rules);

    // -----------------------------------------------------------------------
    // Changes staged for commit (index vs nothing — first commit scenario).
    // For now we compare index vs working tree to show staged/modified/untracked.
    // -----------------------------------------------------------------------

    std::set<std::string> staged_keys;
    for (const auto &[k, _] : staged)
        staged_keys.insert(k);

    std::vector<std::string> new_files;       // in index, not in working tree
    std::vector<std::string> modified;        // in index AND working tree but hash differs
    std::vector<std::string> deleted_staged;  // in index, deleted from working tree
    std::vector<std::string> untracked;       // in working tree, not in index

    // Check every staged file.
    for (const auto &[path, blob_id] : staged)
    {
        const auto abs = repo.root() / path;
        if (!fs::exists(abs))
        {
            deleted_staged.push_back(path);
            continue;
        }

        const std::string wt_hash = hash_file(abs);
        if (wt_hash != blob_id)
            modified.push_back(path);
        // else: up to date
    }

    // Check working tree files not in index.
    for (const auto &wt_path : wt_files)
    {
        if (staged.find(wt_path) == staged.end())
            untracked.push_back(wt_path);
    }

    // -----------------------------------------------------------------------
    // Print output
    // -----------------------------------------------------------------------

    // Determine current branch.
    std::string branch = "HEAD (detached)";
    {
        std::ifstream f(repo.git_dir() / "HEAD");
        std::string raw;
        if (f && std::getline(f, raw))
        {
            if (raw.substr(0, 5) == "ref: ")
            {
                const std::string ref = raw.substr(5);
                // e.g. refs/heads/main → main
                const auto slash = ref.rfind('/');
                branch = (slash != std::string::npos) ? ref.substr(slash + 1) : ref;
                // Strip trailing \r
                if (!branch.empty() && branch.back() == '\r')
                    branch.pop_back();
            }
        }
    }

    std::cout << "On branch " << branch << "\n\n";

    if (!staged.empty())
    {
        std::cout << "Changes to be committed:\n";
        for (const auto &[path, _] : staged)
            std::cout << "\tnew file:   " << path << '\n';
        std::cout << '\n';
    }

    if (!deleted_staged.empty() || !modified.empty())
    {
        std::cout << "Changes not staged for commit:\n";
        for (const auto &p : deleted_staged)
            std::cout << "\tdeleted:    " << p << '\n';
        for (const auto &p : modified)
            std::cout << "\tmodified:   " << p << '\n';
        std::cout << '\n';
    }

    if (!untracked.empty())
    {
        std::cout << "Untracked files:\n";
        for (const auto &p : untracked)
            std::cout << "\t" << p << '\n';
        std::cout << '\n';
    }

    if (staged.empty() && deleted_staged.empty() && modified.empty())
        std::cout << "nothing to commit, working tree clean\n";
}
