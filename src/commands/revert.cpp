#include "revert.h"

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
// Helpers (pattern consistent with the rest of the codebase)
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

// Resolve HEAD to the current commit SHA (empty if no commits yet).
static std::string resolve_head(const fs::path& git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.empty()) return {};
    if (raw.substr(0, 5) == "ref: ")
        return trim(read_text(git_dir / raw.substr(5)));
    return raw;
}

// Resolve a user-supplied target string to a commit SHA.
// Tries: refs/heads/<target>, refs/tags/<target>, then treats as raw SHA.
static std::string resolve_target(const fs::path& git_dir,
                                  const std::string& target)
{
    const fs::path branch = git_dir / "refs" / "heads" / target;
    if (fs::exists(branch)) return trim(read_text(branch));

    const fs::path tag = git_dir / "refs" / "tags" / target;
    if (fs::exists(tag)) return trim(read_text(tag));

    return target; // assume raw SHA
}

// Update the ref pointed to by HEAD (or HEAD directly if detached).
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

// Read a blob body (content after the object-store header).
static std::string blob_body(const ObjectDatabase& db, const std::string& id)
{
    return strip_object_header(db.read(id));
}

// Build a flat path→blobId map from a parsed tree.
static std::unordered_map<std::string, std::string>
tree_to_map(const ParsedTree& tree)
{
    std::unordered_map<std::string, std::string> m;
    for (const auto& e : tree.entries) m[e.name] = e.id;
    return m;
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
// revert_command
// ---------------------------------------------------------------------------

void revert_command(const std::string& target, const std::string& author)
{
    // 1. Discover repository.
    Repository repo = [&]() -> Repository {
        try { return Repository::discover(fs::current_path()); }
        catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    ObjectDatabase db(repo.git_dir() / "objects");

    // 2. Resolve the target to a commit SHA.
    const std::string revert_sha = resolve_target(repo.git_dir(), target);
    if (revert_sha.empty())
    {
        std::cerr << "fatal: ambiguous argument '" << target
                  << "': unknown revision\n";
        std::exit(1);
    }

    // 3. Read and validate the target commit object.
    ParsedCommit revert_commit;
    try
    {
        const std::string raw = db.read(revert_sha);
        if (raw.substr(0, 7) != "commit ")
        {
            std::cerr << "fatal: '" << revert_sha << "' is not a commit object\n";
            std::exit(1);
        }
        revert_commit = parse_commit(raw);
    }
    catch (const std::exception& e)
    {
        std::cerr << "fatal: " << e.what() << '\n';
        std::exit(1);
    }

    // 4. Resolve the current HEAD commit (what we are reverting into).
    const std::string head_sha = resolve_head(repo.git_dir());
    if (head_sha.empty())
    {
        std::cerr << "fatal: HEAD has no commits — nothing to revert into\n";
        std::exit(1);
    }

    // 5. Guard: reverting the current HEAD commit into itself is a no-op edge
    //    case; allow it (creates an empty revert commit, just like git).

    // 6. Obtain the three tree snapshots needed for inverse three-way merge:
    //
    //    base   = the commit being reverted   (revert_sha's tree)
    //    ours   = the current HEAD tree       (what we have now)
    //    theirs = parent of the reverted commit
    //             (the state *before* that commit was made)
    //
    //    After the merge, files that the reverted commit added will be
    //    removed, files it deleted will be restored, and files it modified
    //    will have the modification undone — exactly like `git revert`.

    // Build the "theirs" tree: the parent commit's tree.
    // If the reverted commit is a root commit (no parents), theirs is empty.
    std::unordered_map<std::string, std::string> theirs_map; // path → blobId
    if (!revert_commit.parent_ids.empty())
    {
        try
        {
            const ParsedCommit parent_commit =
                parse_commit(db.read(revert_commit.parent_ids[0]));
            const ParsedTree parent_tree =
                parse_tree(db.read(parent_commit.tree_id));
            theirs_map = tree_to_map(parent_tree);
        }
        catch (const std::exception& e)
        {
            std::cerr << "fatal: could not read parent of reverted commit: "
                      << e.what() << '\n';
            std::exit(1);
        }
    }

    // Build the "base" tree: the reverted commit's own tree.
    std::unordered_map<std::string, std::string> base_map;
    try
    {
        const ParsedTree base_tree = parse_tree(db.read(revert_commit.tree_id));
        base_map = tree_to_map(base_tree);
    }
    catch (const std::exception& e)
    {
        std::cerr << "fatal: could not read tree of reverted commit: "
                  << e.what() << '\n';
        std::exit(1);
    }

    // Build the "ours" tree: the current HEAD's tree.
    std::unordered_map<std::string, std::string> ours_map;
    try
    {
        const ParsedCommit head_commit = parse_commit(db.read(head_sha));
        const ParsedTree   head_tree   = parse_tree(db.read(head_commit.tree_id));
        ours_map = tree_to_map(head_tree);
    }
    catch (const std::exception& e)
    {
        std::cerr << "fatal: could not read HEAD tree: " << e.what() << '\n';
        std::exit(1);
    }

    // 7. Determine the current branch name for conflict markers.
    std::string ours_label;
    {
        const std::string raw = trim(read_text(repo.git_dir() / "HEAD"));
        if (raw.size() > 5 && raw.substr(0, 5) == "ref: ")
        {
            const std::string ref = raw.substr(5);
            const auto slash = ref.rfind('/');
            ours_label = (slash != std::string::npos) ? ref.substr(slash + 1) : ref;
        }
        else
        {
            ours_label = "HEAD";
        }
    }
    const std::string theirs_label = "parent of " + revert_sha.substr(0, 7);

    // 8. Perform the three-way inverse merge across all paths.
    std::unordered_set<std::string> all_paths;
    for (const auto& [p, _] : ours_map)   all_paths.insert(p);
    for (const auto& [p, _] : theirs_map) all_paths.insert(p);
    for (const auto& [p, _] : base_map)   all_paths.insert(p);

    std::unordered_map<std::string, std::string> result_index; // path → blobId
    bool any_conflict = false;

    for (const auto& path : all_paths)
    {
        const auto ours_it   = ours_map.find(path);
        const auto theirs_it = theirs_map.find(path);
        const auto base_it   = base_map.find(path);

        const bool in_ours   = (ours_it   != ours_map.end());
        const bool in_theirs = (theirs_it != theirs_map.end());
        const bool in_base   = (base_it   != base_map.end());

        // If ours and theirs agree already, keep as-is.
        if (in_ours && in_theirs && ours_it->second == theirs_it->second)
        {
            result_index[path] = ours_it->second;
            continue;
        }

        // File exists only in ours.
        if (in_ours && !in_theirs)
        {
            if (!in_base)
            {
                // We added it independently of the reverted commit — keep it.
                result_index[path] = ours_it->second;
            }
            else if (ours_it->second == base_it->second)
            {
                // The reverted commit added this file; we haven't changed it
                // → removing it is the correct inverse action.
                // (do not add to result_index → file is deleted)
            }
            else
            {
                // We modified a file the reverted commit originally added.
                // Report a conflict: the revert wants to delete it, but we
                // have local modifications.
                std::cerr << "CONFLICT (modify/delete): " << path
                          << " deleted in revert and modified in "
                          << ours_label << '\n';
                result_index[path] = ours_it->second;
                any_conflict = true;
            }
            continue;
        }

        // File exists only in theirs (pre-revert state).
        if (!in_ours && in_theirs)
        {
            if (!in_base)
            {
                // Theirs has a file the reverted commit never touched, and we
                // don't have it either — this shouldn't normally occur, but
                // accept it.
                const std::string body = blob_body(db, theirs_it->second);
                write_file(repo.root() / path, body);
                result_index[path] = theirs_it->second;
            }
            else
            {
                // The reverted commit deleted this file; restoring it.
                // Check whether we also deleted it (same as theirs intent) or
                // if there's a potential conflict.
                // Since we don't have it in ours, and theirs wants it back,
                // accept theirs (restore the file).
                const std::string body = blob_body(db, theirs_it->second);
                write_file(repo.root() / path, body);
                result_index[path] = theirs_it->second;
            }
            continue;
        }

        // Both sides have the file — perform content-level three-way merge.
        {
            const std::string base_content =
                in_base ? blob_body(db, base_it->second) : std::string{};
            const std::string ours_content   = blob_body(db, ours_it->second);
            const std::string theirs_content = blob_body(db, theirs_it->second);

            FileMergeResult merge_result = three_way_merge(
                base_content, ours_content, theirs_content,
                ours_label, theirs_label
            );

            write_file(repo.root() / path, merge_result.content);

            Blob result_blob(merge_result.content);
            db.write(result_blob.id(), result_blob.serialized());
            result_index[path] = result_blob.id();

            if (merge_result.status == MergeStatus::Conflict)
            {
                std::cerr << "CONFLICT (content): Merge conflict in " << path << '\n';
                any_conflict = true;
            }
        }
    }

    // 9. Rebuild the index.
    { std::ofstream clr(repo.git_dir() / "index", std::ios::trunc); }
    Index new_index(repo.git_dir() / "index");
    for (const auto& [path, blob_id] : result_index)
        new_index.add(path, blob_id);
    new_index.write();

    // 10. If there were conflicts, stop and ask user to resolve.
    if (any_conflict)
    {
        std::cerr << "\nAutomatic revert failed; fix conflicts and then commit the result.\n";
        std::exit(1);
    }

    // 11. Write the new tree from the merged result.
    std::vector<TreeEntry> tree_entries;
    tree_entries.reserve(result_index.size());
    for (const auto& [path, blob_id] : result_index)
        tree_entries.push_back({"100644", path, blob_id});

    Tree revert_tree(std::move(tree_entries));
    db.write(revert_tree.id(), revert_tree.serialized());

    // 12. Create the revert commit.
    //     Message format: Revert "<original message>"
    const std::string revert_msg =
        "Revert \"" + revert_commit.message + '"';

    Commit revert_commit_obj(
        revert_tree.id(),
        {head_sha},       // single parent: current HEAD
        author,
        revert_msg
    );
    db.write(revert_commit_obj.id(), revert_commit_obj.serialized());

    // 13. Advance the branch pointer.
    update_ref(repo.git_dir(), revert_commit_obj.id());

    std::cout << "[" << revert_commit_obj.id().substr(0, 7) << "] "
              << revert_msg << '\n';
}
