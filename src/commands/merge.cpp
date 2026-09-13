#include "merge.h"

#include "../index/index.h"
#include "../merge/merge_engine.h"
#include "../objects/blob.h"
#include "../objects/commit.h"
#include "../objects/object_database.h"
#include "../objects/object_parser.h"
#include "../objects/tree.h"
#include "../repository/repository.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Local helpers (same pattern used throughout the codebase)
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

// Read the commit SHA that HEAD currently points to.
static std::string resolve_head(const fs::path& git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.empty()) return {};
    if (raw.substr(0, 5) == "ref: ")
        return trim(read_text(git_dir / raw.substr(5)));
    return raw;
}

// Read the current branch name (empty when HEAD is detached).
static std::string current_branch_name(const fs::path& git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.size() > 5 && raw.substr(0, 5) == "ref: ")
    {
        // "ref: refs/heads/main" → "main"
        const std::string ref = raw.substr(5); // "refs/heads/main"
        const auto slash = ref.rfind('/');
        if (slash != std::string::npos)
            return ref.substr(slash + 1);
        return ref;
    }
    return {}; // detached
}

// Write `sha` to the ref that HEAD points to (or directly to HEAD if detached).
static void update_ref(const fs::path& git_dir, const std::string& sha)
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
        ref_path = git_dir / "HEAD";
    }
    std::ofstream f(ref_path, std::ios::trunc);
    if (!f) throw std::runtime_error("Could not update ref: " + ref_path.string());
    f << sha << '\n';
}

// Build a flat path→blobId map from a parsed tree.
static std::unordered_map<std::string, std::string>
tree_to_map(const ParsedTree& tree)
{
    std::unordered_map<std::string, std::string> m;
    for (const auto& e : tree.entries)
        m[e.name] = e.id;
    return m;
}

// Read a blob body (strips the object-store header).
static std::string blob_body(const ObjectDatabase& db, const std::string& id)
{
    return strip_object_header(db.read(id));
}

// Write content to a file, creating parent directories as needed.
static void write_file(const fs::path& abs, const std::string& content)
{
    fs::create_directories(abs.parent_path());
    std::ofstream f(abs, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("Cannot write: " + abs.string());
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

// ---------------------------------------------------------------------------
// merge_command
// ---------------------------------------------------------------------------

void merge_command(const std::string& branch, const std::string& author)
{
    // 1. Discover the repository.
    Repository repo = [&]() -> Repository {
        try { return Repository::discover(fs::current_path()); }
        catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    ObjectDatabase db(repo.git_dir() / "objects");

    // 2. Resolve target branch → commit SHA.
    const fs::path their_ref = repo.git_dir() / "refs" / "heads" / branch;
    if (!fs::exists(their_ref))
    {
        std::cerr << "fatal: branch '" << branch << "' not found.\n";
        std::exit(1);
    }
    const std::string their_sha = trim(read_text(their_ref));
    if (their_sha.empty())
    {
        std::cerr << "fatal: branch '" << branch << "' has no commits.\n";
        std::exit(1);
    }

    // 3. Resolve HEAD → our commit SHA.
    const std::string our_sha = resolve_head(repo.git_dir());
    if (our_sha.empty())
    {
        std::cerr << "fatal: HEAD has no commits — cannot merge.\n";
        std::exit(1);
    }

    // 4. Guard against merging a branch into itself.
    if (our_sha == their_sha)
    {
        std::cout << "Already up to date.\n";
        return;
    }

    const std::string our_branch = current_branch_name(repo.git_dir());

    // 5. Find the merge base (LCA).
    const std::string base_sha = find_merge_base(
        our_sha, their_sha,
        [&](const std::string& sha) { return db.read(sha); }
    );

    // 6. Fast-forward check: if our HEAD *is* the merge base, fast-forward.
    if (base_sha == our_sha)
    {
        // Fast-forward: advance our branch pointer and restore working tree.
        const ParsedCommit their_commit = parse_commit(db.read(their_sha));
        const ParsedTree   their_tree   = parse_tree(db.read(their_commit.tree_id));

        // Update working tree and index.
        { std::ofstream clr(repo.git_dir() / "index", std::ios::trunc); }
        Index fresh_index(repo.git_dir() / "index");
        for (const auto& entry : their_tree.entries)
        {
            const std::string body = blob_body(db, entry.id);
            write_file(repo.root() / entry.name, body);
            fresh_index.add(entry.name, entry.id);
        }
        fresh_index.write();
        update_ref(repo.git_dir(), their_sha);

        std::cout << "Fast-forward\n";
        std::cout << "Updated HEAD to " << their_sha.substr(0, 7)
                  << " (" << their_commit.message << ")\n";
        return;
    }

    // 7. Check: if theirs IS the merge base, we are already up to date.
    if (base_sha == their_sha)
    {
        std::cout << "Already up to date.\n";
        return;
    }

    // 8. Three-way merge.
    //    Collect the three tree snapshots as path→blobId maps.
    const ParsedCommit our_commit   = parse_commit(db.read(our_sha));
    const ParsedCommit their_commit = parse_commit(db.read(their_sha));

    const ParsedTree our_tree   = parse_tree(db.read(our_commit.tree_id));
    const ParsedTree their_tree = parse_tree(db.read(their_commit.tree_id));

    auto our_map   = tree_to_map(our_tree);
    auto their_map = tree_to_map(their_tree);

    std::unordered_map<std::string, std::string> base_map;
    if (!base_sha.empty())
    {
        const ParsedCommit base_commit = parse_commit(db.read(base_sha));
        const ParsedTree   base_tree   = parse_tree(db.read(base_commit.tree_id));
        base_map = tree_to_map(base_tree);
    }

    // Collect the union of all paths across all three trees.
    std::unordered_set<std::string> all_paths;
    for (const auto& [p, _] : our_map)   all_paths.insert(p);
    for (const auto& [p, _] : their_map) all_paths.insert(p);
    for (const auto& [p, _] : base_map)  all_paths.insert(p);

    // New index entries after the merge.
    std::unordered_map<std::string, std::string> merged_index; // path → blob id
    bool any_conflict = false;

    for (const auto& path : all_paths)
    {
        const auto our_it   = our_map.find(path);
        const auto their_it = their_map.find(path);
        const auto base_it  = base_map.find(path);

        const bool in_ours   = (our_it   != our_map.end());
        const bool in_theirs = (their_it != their_map.end());
        const bool in_base   = (base_it  != base_map.end());

        // ---- Cases --------------------------------------------------------

        // Both sides are identical (covers unchanged files).
        if (in_ours && in_theirs && our_it->second == their_it->second)
        {
            merged_index[path] = our_it->second;
            continue;
        }

        // File only in ours (theirs deleted it or it's ours-only new).
        if (in_ours && !in_theirs)
        {
            if (!in_base)
            {
                // Added only by us — keep it.
                merged_index[path] = our_it->second;
            }
            else if (our_it->second == base_it->second)
            {
                // We didn't change it; theirs deleted it — accept deletion.
                // (do not add to merged_index)
            }
            else
            {
                // We changed it; theirs deleted it — conflict.
                std::cerr << "CONFLICT (modify/delete): " << path
                          << " deleted in " << branch
                          << " and modified in "
                          << (our_branch.empty() ? "HEAD" : our_branch) << '\n';
                merged_index[path] = our_it->second; // keep our version
                any_conflict = true;
            }
            continue;
        }

        // File only in theirs.
        if (!in_ours && in_theirs)
        {
            if (!in_base)
            {
                // Added only by theirs — accept it.
                const std::string body = blob_body(db, their_it->second);
                write_file(repo.root() / path, body);
                merged_index[path] = their_it->second;
            }
            else if (their_it->second == base_it->second)
            {
                // Theirs didn't change it; we deleted it — accept deletion.
            }
            else
            {
                // Theirs changed it; we deleted it — conflict.
                std::cerr << "CONFLICT (modify/delete): " << path
                          << " deleted in "
                          << (our_branch.empty() ? "HEAD" : our_branch)
                          << " and modified in " << branch << '\n';
                const std::string body = blob_body(db, their_it->second);
                write_file(repo.root() / path, body);
                merged_index[path] = their_it->second;
                any_conflict = true;
            }
            continue;
        }

        // Both sides have the file but with different blob IDs.
        {
            const std::string base_content =
                in_base ? blob_body(db, base_it->second) : std::string{};
            const std::string our_content   = blob_body(db, our_it->second);
            const std::string their_content = blob_body(db, their_it->second);

            const std::string ours_label =
                our_branch.empty() ? "HEAD" : our_branch;

            FileMergeResult result = three_way_merge(
                base_content, our_content, their_content,
                ours_label, branch
            );

            // Write the (possibly conflict-marked) merged content to disk.
            write_file(repo.root() / path, result.content);

            // Store merged content as a new blob.
            Blob merged_blob(result.content);
            db.write(merged_blob.id(), merged_blob.serialized());
            merged_index[path] = merged_blob.id();

            if (result.status == MergeStatus::Conflict)
            {
                std::cerr << "CONFLICT (content): Merge conflict in " << path << '\n';
                any_conflict = true;
            }
        }
    }

    // 9. Rebuild the index with merged entries.
    { std::ofstream clr(repo.git_dir() / "index", std::ios::trunc); }
    Index new_index(repo.git_dir() / "index");
    for (const auto& [path, blob_id] : merged_index)
        new_index.add(path, blob_id);
    new_index.write();

    // 10. If conflicts exist, stop here and ask the user to resolve.
    if (any_conflict)
    {
        std::cerr << "\nAutomatic merge failed; fix conflicts and then commit the result.\n";
        std::exit(1);
    }

    // 11. Create the merge commit with two parents.
    //     Build a tree from the merged index.
    std::vector<TreeEntry> tree_entries;
    tree_entries.reserve(merged_index.size());
    for (const auto& [path, blob_id] : merged_index)
        tree_entries.push_back({"100644", path, blob_id});

    Tree merge_tree(std::move(tree_entries));
    db.write(merge_tree.id(), merge_tree.serialized());

    const std::string merge_msg =
        "Merge branch '" + branch + "'" +
        (our_branch.empty() ? "" : " into " + our_branch);

    Commit merge_commit(merge_tree.id(), {our_sha, their_sha}, author, merge_msg);
    db.write(merge_commit.id(), merge_commit.serialized());

    update_ref(repo.git_dir(), merge_commit.id());

    std::cout << "Merge made by the 'recursive' strategy.\n";
    std::cout << "[" << merge_commit.id().substr(0, 7) << "] " << merge_msg << '\n';
}
