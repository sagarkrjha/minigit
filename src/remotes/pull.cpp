#include "pull.h"
#include "fetch.h"

#include "config.h"
#include "transfer.h"
#include "repository/repository.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"
#include "staging/index.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

static std::string read_text(const fs::path &p)
{
    std::ifstream f(p);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string trim(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

static std::string current_branch(const fs::path &git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.substr(0, 5) != "ref: ") return {};
    const std::string ref = raw.substr(5);
    const auto slash = ref.rfind('/');
    return (slash != std::string::npos) ? ref.substr(slash + 1) : ref;
}

static std::string resolve_branch(const fs::path &git_dir, const std::string &branch)
{
    return trim(read_text(git_dir / "refs" / "heads" / branch));
}

// Restore all files from a flat tree into the working directory and rebuild idx.
static void apply_tree(const fs::path &root,
                       const fs::path &objects_dir,
                       const std::string &tree_sha,
                       Index &idx)
{
    ObjectDatabase db(objects_dir);
    const auto tree = parse_tree(db.read(tree_sha));
    for (const auto &entry : tree.entries)
    {
        const std::string content = strip_object_header(db.read(entry.id));
        const fs::path    abs     = root / entry.name;
        fs::create_directories(abs.parent_path());
        std::ofstream f(abs, std::ios::binary | std::ios::trunc);
        if (!f) throw std::runtime_error("Cannot write: " + abs.string());
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
        idx.add(entry.name, entry.id);
    }
}

void pull_command(const std::string &remote_name_in, const std::string &branch_name_in)
{
    Repository local = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    const std::string remote_name = remote_name_in.empty() ? "origin" : remote_name_in;

    // Determine branch.
    std::string branch = branch_name_in;
    if (branch.empty())
    {
        branch = current_branch(local.git_dir());
        if (branch.empty())
        {
            std::cerr << "error: cannot pull in detached HEAD state. "
                         "Specify a branch name.\n";
            std::exit(1);
        }
    }

    // -----------------------------------------------------------------------
    // Step 1: fetch
    // -----------------------------------------------------------------------
    fetch_command(remote_name);

    // -----------------------------------------------------------------------
    // Step 2: determine commits
    // -----------------------------------------------------------------------
    const std::string local_sha = resolve_branch(local.git_dir(), branch);

    const fs::path tracking_ref =
        local.git_dir() / "refs" / "remotes" / remote_name / branch;
    const std::string remote_sha = trim(read_text(tracking_ref));

    if (remote_sha.empty())
    {
        std::cerr << "error: no tracking ref for "
                  << remote_name << "/" << branch << '\n';
        std::exit(1);
    }

    if (local_sha == remote_sha)
    {
        std::cout << "Already up to date.\n";
        return;
    }

    // -----------------------------------------------------------------------
    // Step 3: fast-forward only
    // -----------------------------------------------------------------------
    const fs::path objects_dir = local.git_dir() / "objects";

    if (!local_sha.empty() &&
        !transfer::is_ancestor(objects_dir, local_sha, remote_sha))
    {
        std::cerr << "error: pull aborted — remote branch has diverged.\n";
        std::cerr << "hint: use 'minigit merge " << remote_name << "/" << branch
                  << "' to merge manually.\n";
        std::exit(1);
    }

    // Fast-forward: update local branch ref.
    const fs::path branch_ref = local.git_dir() / "refs" / "heads" / branch;
    {
        std::ofstream f(branch_ref, std::ios::trunc);
        if (!f)
            throw std::runtime_error("Cannot update branch ref: " +
                                     branch_ref.string());
        f << remote_sha << '\n';
    }

    // Restore working tree from the new commit's tree.
    const auto new_commit = parse_commit(
        ObjectDatabase(objects_dir).read(remote_sha));

    // Clear index and rewrite.
    {
        std::ofstream clear(local.git_dir() / "index", std::ios::trunc);
    }
    Index idx(local.git_dir() / "index");
    apply_tree(local.root(), objects_dir, new_commit.tree_id, idx);
    idx.write();

    const std::string from = local_sha.empty() ? "(empty)" : local_sha.substr(0, 7);
    std::cout << "Fast-forward " << from << ".."
              << remote_sha.substr(0, 7) << '\n';
    std::cout << "Updated branch '" << branch << "'.\n";
}
