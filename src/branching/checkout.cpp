#include "checkout.h"

#include "staging/index.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"
#include "repository/repository.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

static std::string read_text(const fs::path &path)
{
    std::ifstream f(path);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string trim_trailing(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

// Resolve `target` to a commit SHA.
// Tries: refs/heads/<target>, then treats as raw SHA.
static std::string resolve_to_commit_sha(const fs::path &git_dir,
                                         const std::string &target,
                                         bool &is_branch)
{
    const fs::path branch_ref = Repository::resolve_path(git_dir, "refs/heads/" + target);
    if (fs::exists(branch_ref))
    {
        is_branch = true;
        return trim_trailing(read_text(branch_ref));
    }

    is_branch = false;
    return target; // assume it's a raw SHA
}

// Restore a single file from a blob.
static void restore_file(const fs::path &root,
                          const std::string &rel_path,
                          const std::string &content)
{
    const fs::path abs = root / rel_path;
    fs::create_directories(abs.parent_path());
    std::ofstream f(abs, std::ios::binary | std::ios::trunc);
    if (!f)
        throw std::runtime_error("Cannot write file: " + abs.string());
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

void checkout_command(const std::string &target)
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    ObjectDatabase db(repo.git_dir() / "objects");

    bool is_branch = false;
    const std::string commit_sha =
        resolve_to_commit_sha(repo.git_dir(), target, is_branch);

    if (commit_sha.empty())
    {
        std::cerr << "error: pathspec '" << target
                  << "' did not match any branch or commit\n";
        std::exit(1);
    }

    if (is_branch)
    {
        auto match = repo.find_branch_worktree(target);
        if (match.is_checked_out && !match.is_current_worktree)
        {
            std::cerr << "fatal: '" << target << "' is already checked out at '"
                      << match.worktree_path.generic_string() << "'\n";
            std::exit(1);
        }
    }

    // Read commit → tree.
    ParsedCommit commit;
    ParsedTree   tree;
    try
    {
        commit = parse_commit(db.read(commit_sha));
        tree   = parse_tree(db.read(commit.tree_id));
    }
    catch (const std::exception &e)
    {
        std::cerr << "error: " << e.what() << '\n';
        std::exit(1);
    }

    // Restore each file in the tree.
    for (const auto &entry : tree.entries)
    {
        if (entry.mode == "160000")
        {
            // Submodule gitlink: do not overwrite directory with blob
            continue;
        }
        std::string content;
        try
        {
            content = strip_object_header(db.read(entry.id));
        }
        catch (const std::exception &e)
        {
            std::cerr << "error restoring '" << entry.name
                      << "': " << e.what() << '\n';
            continue;
        }
        restore_file(repo.root(), entry.name, content);
    }

    // Rebuild the index to match the checked-out tree.
    Index new_index(repo.git_dir() / "index");
    // Clear existing entries by overwriting with the tree's entries.
    // We load a fresh Index (which starts empty if we could clear it).
    // Trick: write an empty index first, then load and populate.
    {
        std::ofstream clear_index(repo.git_dir() / "index", std::ios::trunc);
    }
    Index fresh_index(repo.git_dir() / "index");
    for (const auto &entry : tree.entries)
        fresh_index.add(entry.name, entry.id);
    fresh_index.write();

    // Update HEAD.
    const fs::path head_path = repo.git_dir() / "HEAD";
    std::ofstream head_file(head_path, std::ios::trunc);
    if (!head_file)
        throw std::runtime_error("Could not update HEAD");

    if (is_branch)
    {
        head_file << "ref: refs/heads/" << target << '\n';
        std::cout << "Switched to branch '" << target << "'\n";
    }
    else
    {
        head_file << commit_sha << '\n';
        std::cout << "HEAD is now at " << commit_sha.substr(0, 7)
                  << ' ' << commit.message << '\n';
    }
}
