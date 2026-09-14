#include "stash.h"

#include "../hashing/sha256.h"
#include "../ignore/ignore.h"
#include "../index/index.h"
#include "../objects/blob.h"
#include "../objects/commit.h"
#include "../objects/object_database.h"
#include "../objects/object_parser.h"
#include "../objects/tree.h"
#include "../repository/repository.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Internal helpers
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

// Resolve HEAD to the current commit SHA (empty string if no commits yet).
static std::string resolve_head(const fs::path& git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.empty()) return {};
    if (raw.size() >= 5 && raw.substr(0, 5) == "ref: ")
        return trim(read_text(git_dir / raw.substr(5)));
    return raw;
}

// Return the current branch name, or "HEAD" if detached.
static std::string current_branch(const fs::path& git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.size() > 5 && raw.substr(0, 5) == "ref: ")
    {
        const std::string ref = raw.substr(5);
        const auto slash = ref.rfind('/');
        return (slash != std::string::npos) ? ref.substr(slash + 1) : ref;
    }
    return "HEAD";
}

// Build a flat path→blobId map from a parsed tree.
static std::unordered_map<std::string, std::string>
tree_to_map(const ParsedTree& tree)
{
    std::unordered_map<std::string, std::string> m;
    for (const auto& e : tree.entries) m[e.name] = e.id;
    return m;
}

// Read the blob body (bytes after the object-store header).
static std::string blob_body(const ObjectDatabase& db, const std::string& id)
{
    return strip_object_header(db.read(id));
}

// Write file content to disk, creating parent directories as needed.
static void write_file(const fs::path& abs, const std::string& content)
{
    fs::create_directories(abs.parent_path());
    std::ofstream f(abs, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("Cannot write: " + abs.string());
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

// ---------------------------------------------------------------------------
// Stash stack persistence: .minigit/stash
// One commit SHA per line, index 0 = most recent.
// ---------------------------------------------------------------------------

static std::vector<std::string> read_stash_list(const fs::path& git_dir)
{
    std::vector<std::string> list;
    std::ifstream f(git_dir / "stash");
    if (!f) return list;
    std::string line;
    while (std::getline(f, line))
    {
        // strip \r in case of CRLF
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) list.push_back(line);
    }
    return list;
}

static void write_stash_list(const fs::path& git_dir,
                              const std::vector<std::string>& list)
{
    std::ofstream f(git_dir / "stash", std::ios::trunc);
    if (!f) throw std::runtime_error("Cannot write stash list");
    for (const auto& sha : list)
        f << sha << '\n';
}

// Parse stash@{N} → N.  Returns 0 for anything it cannot parse.
static int parse_stash_index(const std::string& ref)
{
    const auto lb = ref.find('{');
    const auto rb = ref.find('}');
    if (lb == std::string::npos || rb == std::string::npos || rb <= lb + 1)
        return 0;
    try { return std::stoi(ref.substr(lb + 1, rb - lb - 1)); }
    catch (...) { return 0; }
}

// ---------------------------------------------------------------------------
// Sub-command implementations
// ---------------------------------------------------------------------------

// minigit stash [push]
// Snapshot working directory → stash commit, then restore HEAD state.
static void stash_push(const Repository& repo, ObjectDatabase& db)
{
    const fs::path git_dir = repo.git_dir();
    const fs::path root    = repo.root();

    // ── 1. Resolve HEAD ────────────────────────────────────────────────────
    const std::string head_sha = resolve_head(git_dir);
    if (head_sha.empty())
    {
        std::cerr << "fatal: you have no commits yet — nothing to stash onto\n";
        std::exit(1);
    }

    // ── 2. Parse HEAD commit & its tree ───────────────────────────────────
    ParsedCommit head_commit;
    ParsedTree   head_tree;
    try
    {
        head_commit = parse_commit(db.read(head_sha));
        head_tree   = parse_tree(db.read(head_commit.tree_id));
    }
    catch (const std::exception& e)
    {
        std::cerr << "fatal: " << e.what() << '\n';
        std::exit(1);
    }

    // ── 3. Snapshot the working directory ─────────────────────────────────
    // Collect every tracked + modified file.  We scan the full working tree
    // (honouring .minigitignore) to capture both staged and unstaged changes.
    const IgnoreRules ignore_rules = IgnoreRules::load(root);

    std::vector<TreeEntry> snap_entries;
    bool any_dirty = false;

    // Build a head-tree map for dirty-detection.
    const auto head_map = tree_to_map(head_tree);

    for (const auto& dir_entry : fs::recursive_directory_iterator(root))
    {
        if (!dir_entry.is_regular_file()) continue;
        const auto rel     = fs::relative(dir_entry.path(), root);
        const std::string rel_str = rel.generic_string();

        if (rel_str.starts_with(".minigit") || rel_str.starts_with(".git"))
            continue;
        if (rel_str == ".minigitignore") continue;
        if (ignore_rules.is_ignored(rel_str)) continue;

        // Read content and create a blob.
        std::ifstream fi(dir_entry.path(), std::ios::binary);
        if (!fi) continue;
        std::string content{
            std::istreambuf_iterator<char>{fi},
            std::istreambuf_iterator<char>{}};

        Blob blob(content);
        const std::string blob_id = blob.id();

        // Check if this file is dirty relative to HEAD.
        auto it = head_map.find(rel_str);
        if (it == head_map.end() || it->second != blob_id)
            any_dirty = true;

        // Write the blob to the object store (noop if already present).
        db.write(blob_id, blob.serialized());

        snap_entries.push_back({"100644", rel_str, blob_id});
    }

    if (!any_dirty)
    {
        std::cout << "No local changes to save\n";
        return;
    }

    // ── 4. Build the stash tree & commit ──────────────────────────────────
    Tree stash_tree(std::move(snap_entries));
    db.write(stash_tree.id(), stash_tree.serialized());

    const std::string branch = current_branch(git_dir);
    const std::string stash_msg =
        "WIP on " + branch + ": " + head_sha.substr(0, 7)
        + " " + head_commit.message;

    Commit stash_commit(
        stash_tree.id(),
        {head_sha},
        "MiniGit User <user@minigit>",
        stash_msg
    );
    db.write(stash_commit.id(), stash_commit.serialized());

    // ── 5. Prepend to stash list ───────────────────────────────────────────
    auto stash_list = read_stash_list(git_dir);
    stash_list.insert(stash_list.begin(), stash_commit.id());
    write_stash_list(git_dir, stash_list);

    // ── 6. Reset index to HEAD's tree ─────────────────────────────────────
    { std::ofstream clr(git_dir / "index", std::ios::trunc); }
    Index fresh_index(git_dir / "index");
    for (const auto& e : head_tree.entries)
        fresh_index.add(e.name, e.id);
    fresh_index.write();

    // ── 7. Restore working tree to HEAD state ─────────────────────────────
    // First, collect all files currently on disk that are tracked (may include
    // new/untracked files that we do NOT delete — same as git stash behaviour).
    for (const auto& e : head_tree.entries)
    {
        try
        {
            const std::string body = blob_body(db, e.id);
            write_file(root / e.name, body);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "warning: could not restore '" << e.name
                      << "': " << ex.what() << '\n';
        }
    }

    std::cout << "Saved working directory and index state " << stash_msg << '\n';
}

// minigit stash list
static void stash_list(const Repository& repo, const ObjectDatabase& db)
{
    const auto list = read_stash_list(repo.git_dir());
    if (list.empty())
    {
        // Print nothing (same as git stash list on an empty stash).
        return;
    }
    for (std::size_t i = 0; i < list.size(); ++i)
    {
        std::string msg;
        try
        {
            const ParsedCommit c = parse_commit(db.read(list[i]));
            msg = c.message;
        }
        catch (...)
        {
            msg = list[i].substr(0, 7) + " (unreadable)";
        }
        std::cout << "stash@{" << i << "}: " << msg << '\n';
    }
}

// minigit stash pop [stash@{N}]
// Apply the Nth stash entry to the working tree + index, then drop it.
static void stash_pop(const Repository& repo, ObjectDatabase& db,
                      const std::string& stash_ref)
{
    const fs::path git_dir = repo.git_dir();
    const int idx = parse_stash_index(stash_ref);

    auto list = read_stash_list(git_dir);
    if (list.empty())
    {
        std::cerr << "error: No stash entries found.\n";
        std::exit(1);
    }
    if (idx < 0 || static_cast<std::size_t>(idx) >= list.size())
    {
        std::cerr << "error: " << stash_ref << " is not a valid reference\n";
        std::exit(1);
    }

    const std::string stash_sha = list[static_cast<std::size_t>(idx)];

    // Parse the stash commit's tree.
    ParsedTree stash_tree;
    try
    {
        const ParsedCommit c = parse_commit(db.read(stash_sha));
        stash_tree = parse_tree(db.read(c.tree_id));
    }
    catch (const std::exception& e)
    {
        std::cerr << "fatal: cannot read stash entry: " << e.what() << '\n';
        std::exit(1);
    }

    // Write files to disk.
    for (const auto& e : stash_tree.entries)
    {
        try
        {
            const std::string body = blob_body(db, e.id);
            write_file(repo.root() / e.name, body);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "warning: could not restore '" << e.name
                      << "': " << ex.what() << '\n';
        }
    }

    // Update the index.
    { std::ofstream clr(git_dir / "index", std::ios::trunc); }
    Index fresh_index(git_dir / "index");
    for (const auto& e : stash_tree.entries)
        fresh_index.add(e.name, e.id);
    fresh_index.write();

    // Remove entry from the stash list.
    list.erase(list.begin() + idx);
    write_stash_list(git_dir, list);

    std::cout << "Dropped " << stash_ref << " (" << stash_sha.substr(0, 7) << ")\n";
}

// minigit stash drop [stash@{N}]
// Discard the Nth stash entry without applying it.
static void stash_drop(const Repository& repo, const std::string& stash_ref)
{
    const fs::path git_dir = repo.git_dir();
    const int idx = parse_stash_index(stash_ref);

    auto list = read_stash_list(git_dir);
    if (list.empty())
    {
        std::cerr << "error: No stash entries found.\n";
        std::exit(1);
    }
    if (idx < 0 || static_cast<std::size_t>(idx) >= list.size())
    {
        std::cerr << "error: " << stash_ref << " is not a valid reference\n";
        std::exit(1);
    }

    const std::string stash_sha = list[static_cast<std::size_t>(idx)];
    list.erase(list.begin() + idx);
    write_stash_list(git_dir, list);

    std::cout << "Dropped " << stash_ref << " (" << stash_sha.substr(0, 7) << ")\n";
}

// minigit stash show [stash@{N}]
// Show the list of files in the Nth stash entry.
static void stash_show(const Repository& repo, const ObjectDatabase& db,
                       const std::string& stash_ref)
{
    const int idx = parse_stash_index(stash_ref);

    const auto list = read_stash_list(repo.git_dir());
    if (list.empty())
    {
        std::cerr << "error: No stash entries found.\n";
        std::exit(1);
    }
    if (idx < 0 || static_cast<std::size_t>(idx) >= list.size())
    {
        std::cerr << "error: " << stash_ref << " is not a valid reference\n";
        std::exit(1);
    }

    const std::string stash_sha = list[static_cast<std::size_t>(idx)];

    ParsedTree stash_tree;
    try
    {
        const ParsedCommit c = parse_commit(db.read(stash_sha));
        stash_tree = parse_tree(db.read(c.tree_id));
    }
    catch (const std::exception& e)
    {
        std::cerr << "fatal: cannot read stash entry: " << e.what() << '\n';
        std::exit(1);
    }

    for (const auto& e : stash_tree.entries)
        std::cout << e.mode << ' ' << e.id.substr(0, 7) << ' ' << e.name << '\n';
}

// ---------------------------------------------------------------------------
// stash_command — dispatcher
// ---------------------------------------------------------------------------

void stash_command(const std::string& subcommand, const std::string& stash_ref)
{
    Repository repo = [&]() -> Repository {
        try { return Repository::discover(fs::current_path()); }
        catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    ObjectDatabase db(repo.git_dir() / "objects");

    using StashHandler = std::function<void(Repository&, ObjectDatabase&, const std::string&)>;
    static const std::unordered_map<std::string, StashHandler> handlers = {
        {"",     [](Repository& r, ObjectDatabase& d, const std::string& /*ref*/) { stash_push(r, d); }},
        {"push", [](Repository& r, ObjectDatabase& d, const std::string& /*ref*/) { stash_push(r, d); }},
        {"list", [](Repository& r, ObjectDatabase& d, const std::string& /*ref*/) { stash_list(r, d); }},
        {"pop",  [](Repository& r, ObjectDatabase& d, const std::string& ref) { stash_pop(r, d, ref); }},
        {"drop", [](Repository& r, ObjectDatabase& /*d*/, const std::string& ref) { stash_drop(r, ref); }},
        {"show", [](Repository& r, ObjectDatabase& d, const std::string& ref) { stash_show(r, d, ref); }}
    };

    const auto it = handlers.find(subcommand);
    if (it == handlers.end())
    {
        std::cerr << "error: unknown stash subcommand '" << subcommand << "'\n"
                  << "usage: minigit stash [push | list | pop | drop | show] [stash@{N}]\n";
        std::exit(1);
    }

    it->second(repo, db, stash_ref);
}
