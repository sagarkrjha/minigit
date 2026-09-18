#include "clone.h"
#include "smart_http.h"

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

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

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

// Resolve HEAD in src to a commit SHA (empty string = no commits yet).
static std::string resolve_head_sha(const fs::path &git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.empty()) return {};
    if (raw.substr(0, 5) == "ref: ")
        return trim(read_text(git_dir / raw.substr(5)));
    return raw;
}

// Determine the branch name HEAD points to (empty = detached or none).
static std::string head_branch_name(const fs::path &git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.substr(0, 5) != "ref: ") return {};
    const std::string ref = raw.substr(5);
    const auto slash = ref.rfind('/');
    return (slash != std::string::npos) ? ref.substr(slash + 1) : ref;
}

// Restore working tree from a tree object (flat tree).
static void checkout_tree(const fs::path &root,
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

// ---------------------------------------------------------------------------
// Public command
// ---------------------------------------------------------------------------

void clone_command(const std::string &src_path, const std::string &dest_dir_str)
{
    if (minigit::remotes::is_http_url(src_path))
    {
        minigit::remotes::clone_http(src_path, dest_dir_str);
        return;
    }

    // 1. Validate source.
    const fs::path src = fs::weakly_canonical(fs::absolute(src_path));
    const fs::path src_git = src / ".minigit";
    if (!fs::exists(src_git) || !fs::is_directory(src_git))
    {
        std::cerr << "fatal: '" << src_path
                  << "' is not a minigit repository\n";
        std::exit(1);
    }

    // 2. Determine destination directory.
    fs::path dest;
    if (dest_dir_str.empty())
    {
        dest = fs::current_path() / src.filename();
    }
    else
    {
        dest = fs::weakly_canonical(fs::absolute(dest_dir_str));
    }

    if (fs::exists(dest) && !fs::is_empty(dest))
    {
        std::cerr << "fatal: destination '" << dest.string()
                  << "' already exists and is not empty\n";
        std::exit(1);
    }

    std::cout << "Cloning into '" << dest.filename().string() << "'...\n";

    // 3. Initialise new repository.
    fs::create_directories(dest);
    Repository new_repo(dest);
    new_repo.init();

    const fs::path dst_git     = new_repo.git_dir();
    const fs::path src_objects = src_git / "objects";
    const fs::path dst_objects = dst_git / "objects";

    // 4. Discover what HEAD points to in source.
    const std::string branch     = head_branch_name(src_git);
    const std::string commit_sha = resolve_head_sha(src_git);

    if (commit_sha.empty())
    {
        std::cout << "warning: cloned an empty repository.\n";
        // Still register origin remote.
        RemoteConfig cfg(dst_git / "config");
        cfg.add("origin", src.string());
        return;
    }

    // 5. Copy all reachable objects.
    const auto needed = transfer::missing_objects(src_objects, dst_objects, commit_sha);
    transfer::transfer_objects(src_objects, dst_objects, needed);
    std::cout << "Transferred " << needed.size() << " object(s).\n";

    // 6. Copy all branch refs from source.
    const fs::path src_heads = src_git / "refs" / "heads";
    const fs::path dst_heads = dst_git / "refs" / "heads";
    fs::create_directories(dst_heads);

    if (fs::exists(src_heads))
    {
        for (const auto &entry : fs::directory_iterator(src_heads))
        {
            if (!entry.is_regular_file()) continue;
            fs::copy_file(entry.path(), dst_heads / entry.path().filename(),
                          fs::copy_options::overwrite_existing);
        }
    }

    // 7. Copy all tag refs.
    const fs::path src_tags = src_git / "refs" / "tags";
    const fs::path dst_tags = dst_git / "refs" / "tags";
    fs::create_directories(dst_tags);
    if (fs::exists(src_tags))
    {
        for (const auto &entry : fs::directory_iterator(src_tags))
        {
            if (!entry.is_regular_file()) continue;
            fs::copy_file(entry.path(), dst_tags / entry.path().filename(),
                          fs::copy_options::overwrite_existing);
        }
    }

    // 8. Set up tracking remote-tracking refs  (refs/remotes/origin/<branch>).
    if (!branch.empty())
    {
        const fs::path remote_ref = dst_git / "refs" / "remotes" / "origin" / branch;
        fs::create_directories(remote_ref.parent_path());
        std::ofstream f(remote_ref, std::ios::trunc);
        f << commit_sha << '\n';
    }

    // 9. Register origin remote in config.
    RemoteConfig cfg(dst_git / "config");
    cfg.add("origin", src.string());

    // 10. Write HEAD.
    {
        const fs::path head_path = dst_git / "HEAD";
        std::ofstream  head(head_path, std::ios::trunc);
        if (!branch.empty())
            head << "ref: refs/heads/" << branch << '\n';
        else
            head << commit_sha << '\n';
    }

    // 11. Checkout the working tree and build the index.
    {
        const auto commit = parse_commit(
            ObjectDatabase(dst_objects).read(commit_sha));
        Index idx(dst_git / "index");
        checkout_tree(dest, dst_objects, commit.tree_id, idx);
        idx.write();
    }

    if (!branch.empty())
        std::cout << "Branch '" << branch << "' set up to track origin/"
                  << branch << ".\n";
    std::cout << "Done.\n";
}
