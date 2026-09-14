#include "reset.h"

#include "../filesystem/file.h"
#include "../index/index.h"
#include "../objects/object_database.h"
#include "../objects/object_parser.h"
#include "../repository/repository.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Internal helpers (same pattern used across commit/checkout/branch)
// ---------------------------------------------------------------------------

static std::string read_text(const fs::path& path)
{
    std::ifstream f(path);
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

// Resolve `target` to a commit SHA.
// Checks: refs/heads/<target>, refs/tags/<target>, then treats as raw SHA.
static std::string resolve_target(const fs::path& git_dir,
                                  const std::string& target)
{
    // Branch ref?
    const fs::path branch = git_dir / "refs" / "heads" / target;
    if (fs::exists(branch))
        return trim(read_text(branch));

    // Tag ref?
    const fs::path tag = git_dir / "refs" / "tags" / target;
    if (fs::exists(tag))
    {
        const std::string tag_sha = trim(read_text(tag));
        // Annotated tags point to a tag object, not a commit. Follow if needed.
        return tag_sha;
    }

    // Assume it's already a commit SHA.
    return target;
}

// Update the branch ref that HEAD currently points to (or HEAD itself if
// detached) to the given commit SHA.
static void update_ref(const fs::path& git_dir, const std::string& commit_sha)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));

    fs::path ref_path;
    if (raw.size() >= 5 && raw.substr(0, 5) == "ref: ")
    {
        ref_path = git_dir / raw.substr(5);
        fs::create_directories(ref_path.parent_path());
    }
    else
    {
        // Detached HEAD — update HEAD directly.
        ref_path = git_dir / "HEAD";
    }

    std::ofstream f(ref_path, std::ios::trunc);
    if (!f)
        throw std::runtime_error("Could not update ref: " + ref_path.string());
    f << commit_sha << '\n';
}

// Reset the index to exactly match the entries in `tree`.
static void reset_index(const fs::path& git_dir, const ParsedTree& tree)
{
    // Truncate the existing index file, then rebuild from the tree.
    { std::ofstream clear(git_dir / "index", std::ios::trunc); }
    Index fresh(git_dir / "index");
    for (const auto& entry : tree.entries)
        fresh.add(entry.name, entry.id);
    fresh.write();
}

// Restore a single file from a blob body (bytes after the object header).
static void restore_file(const fs::path& root,
                         const std::string& rel_path,
                         const std::string& body)
{
    const fs::path abs = root / rel_path;
    fs::create_directories(abs.parent_path());
    std::ofstream f(abs, std::ios::binary | std::ios::trunc);
    if (!f)
        throw std::runtime_error("Cannot write file: " + abs.string());
    f.write(body.data(), static_cast<std::streamsize>(body.size()));
}

// ---------------------------------------------------------------------------
// reset_command
// ---------------------------------------------------------------------------

void reset_command(const std::string& mode, const std::string& target)
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    ObjectDatabase db(repo.git_dir() / "objects");

    // Resolve the target to a commit SHA.
    const std::string commit_sha = resolve_target(repo.git_dir(), target);
    if (commit_sha.empty())
    {
        std::cerr << "fatal: ambiguous argument '" << target
                  << "': unknown revision\n";
        std::exit(1);
    }

    // Read and parse the target commit.
    ParsedCommit commit;
    ParsedTree   tree;
    try
    {
        const std::string raw_commit = db.read(commit_sha);
        // Verify the object is actually a commit.
        const auto null_pos = raw_commit.find('\0');
        if (null_pos == std::string::npos || raw_commit.substr(0, 7) != "commit ")
        {
            std::cerr << "fatal: '" << commit_sha
                      << "' is not a commit object\n";
            std::exit(1);
        }
        commit = parse_commit(raw_commit);
        tree   = parse_tree(db.read(commit.tree_id));
    }
    catch (const std::exception& e)
    {
        std::cerr << "fatal: " << e.what() << '\n';
        std::exit(1);
    }

    // -----------------------------------------------------------------------
    // Apply the reset according to the requested mode.
    // -----------------------------------------------------------------------

    using ResetHandler = std::function<void(const Repository&, ObjectDatabase&, const std::string&, const ParsedCommit&, const ParsedTree&)>;

    auto handle_soft = [](const Repository& r, ObjectDatabase&, const std::string& sha, const ParsedCommit& c, const ParsedTree&) {
        // Move HEAD / branch pointer only — index and working tree untouched.
        update_ref(r.git_dir(), sha);
        std::cout << "HEAD is now at " << sha.substr(0, 7)
                  << ' ' << c.message << '\n';
    };

    auto handle_mixed = [](const Repository& r, ObjectDatabase&, const std::string& sha, const ParsedCommit& c, const ParsedTree& t) {
        // Move HEAD + reset index; leave working tree alone.
        update_ref(r.git_dir(), sha);
        reset_index(r.git_dir(), t);
        std::cout << "HEAD is now at " << sha.substr(0, 7)
                  << ' ' << c.message << '\n';
        std::cout << "Unstaged changes after reset:\n";
        for (const auto& entry : t.entries)
            std::cout << "M\t" << entry.name << '\n';
    };

    auto handle_hard = [](const Repository& r, ObjectDatabase& d, const std::string& sha, const ParsedCommit& c, const ParsedTree& t) {
        // Move HEAD + reset index + restore working tree.
        update_ref(r.git_dir(), sha);
        reset_index(r.git_dir(), t);

        for (const auto& entry : t.entries)
        {
            try
            {
                const std::string body =
                    strip_object_header(d.read(entry.id));
                restore_file(r.root(), entry.name, body);
            }
            catch (const std::exception& e)
            {
                std::cerr << "warning: could not restore '"
                          << entry.name << "': " << e.what() << '\n';
            }
        }

        std::cout << "HEAD is now at " << sha.substr(0, 7)
                  << ' ' << c.message << '\n';
    };

    static const std::unordered_map<std::string, ResetHandler> mode_handlers = {
        {"--soft",  handle_soft},
        {"--mixed", handle_mixed},
        {"",        handle_mixed},
        {"--hard",  handle_hard}
    };

    const auto it = mode_handlers.find(mode);
    if (it == mode_handlers.end())
    {
        std::cerr << "error: unknown reset mode '" << mode << "'\n"
                  << "usage: minigit reset [--soft | --mixed | --hard] <commit>\n";
        std::exit(1);
    }

    it->second(repo, db, commit_sha, commit, tree);
}
