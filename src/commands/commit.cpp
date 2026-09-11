#include "commit.h"
#include "write_tree.h"

#include "../index/index.h"
#include "../objects/commit.h"
#include "../objects/object_database.h"
#include "../objects/tree.h"
#include "../repository/repository.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Read the raw content of a file; returns empty string if missing.
static std::string read_text(const std::filesystem::path &path)
{
    std::ifstream f(path);
    if (!f)
        return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Strip trailing whitespace/newlines.
static std::string trim(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

// Resolve HEAD to the SHA-256 of the current commit (empty if none).
// HEAD contains either "ref: refs/heads/<branch>" or a raw SHA.
static std::string resolve_head(const std::filesystem::path &git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.empty())
        return {};

    // Symbolic ref: "ref: refs/heads/main"
    if (raw.substr(0, 5) == "ref: ")
    {
        const std::string ref_path = raw.substr(5); // e.g. refs/heads/main
        const std::string sha = trim(read_text(git_dir / ref_path));
        return sha;
    }

    // Detached HEAD: raw SHA
    return raw;
}

// Write `sha` to the branch that HEAD currently points to.
static void update_ref(const std::filesystem::path &git_dir,
                       const std::string &sha)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));

    std::filesystem::path ref_path;
    if (raw.substr(0, 5) == "ref: ")
    {
        ref_path = git_dir / raw.substr(5);
        std::filesystem::create_directories(ref_path.parent_path());
    }
    else
    {
        // Detached HEAD: update HEAD directly.
        ref_path = git_dir / "HEAD";
    }

    std::ofstream f(ref_path, std::ios::trunc);
    if (!f)
        throw std::runtime_error("Could not update ref: " + ref_path.string());
    f << sha << '\n';
}

// ---------------------------------------------------------------------------
// Public command
// ---------------------------------------------------------------------------

void commit(const std::string &message, const std::string &author)
{
    if (message.empty())
    {
        std::cerr << "error: empty commit message\n";
        std::exit(1);
    }

    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(std::filesystem::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    // 1. Snapshot the index into a tree object.
    Index index(repo.git_dir() / "index");
    if (index.entries().empty())
    {
        std::cerr << "error: nothing to commit — index is empty\n";
        std::exit(1);
    }

    std::vector<TreeEntry> tree_entries;
    tree_entries.reserve(index.entries().size());
    for (const auto &[path, blob_id] : index.entries())
        tree_entries.push_back({"100644", path, blob_id});

    Tree tree(std::move(tree_entries));

    ObjectDatabase db(repo.git_dir() / "objects");
    db.write(tree.id(), tree.serialized());

    // 2. Collect parent commits.
    std::vector<std::string> parents;
    const std::string parent_sha = resolve_head(repo.git_dir());
    if (!parent_sha.empty())
        parents.push_back(parent_sha);

    // 3. Build and persist the commit object.
    Commit commit_obj(tree.id(), parents, author, message);
    db.write(commit_obj.id(), commit_obj.serialized());

    // 4. Advance HEAD / branch ref.
    update_ref(repo.git_dir(), commit_obj.id());

    // 5. Report.
    const bool is_root = parents.empty();
    std::cout << "[" << (is_root ? "(root-commit) " : "") << commit_obj.id().substr(0, 7)
              << "] " << message << '\n';
}
