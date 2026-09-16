#include "cherry_pick.h"

#include "merge_engine.h"
#include "repository/repository.h"
#include "staging/index.h"
#include "storage/blob.h"
#include "storage/commit.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"
#include "storage/tree.h"

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
// Helpers
// ---------------------------------------------------------------------------

static std::string read_text(const fs::path& path)
{
    std::ifstream f(path, std::ios::binary);
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

static std::string first_line(const std::string& s)
{
    const size_t pos = s.find('\n');
    if (pos == std::string::npos) return trim(s);
    return trim(s.substr(0, pos));
}

// Resolve HEAD to the current commit SHA (empty if no commits yet).
static std::string resolve_head(const fs::path& git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.empty()) return {};
    if (raw.rfind("ref: ", 0) == 0)
        return trim(read_text(git_dir / raw.substr(5)));
    return raw;
}

// Determine active branch name (or "HEAD" if detached).
static std::string current_branch_name(const fs::path& git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.rfind("ref: refs/heads/", 0) == 0)
        return raw.substr(16);
    if (raw.rfind("ref: ", 0) == 0)
    {
        const std::string ref = raw.substr(5);
        const auto slash = ref.rfind('/');
        return (slash != std::string::npos) ? ref.substr(slash + 1) : ref;
    }
    return "HEAD";
}

// Resolve user-supplied target (branch, tag, full SHA, short SHA prefix) to commit SHA.
static std::string resolve_commit_target(const fs::path& git_dir, const std::string& target)
{
    // 1. Branch ref?
    const fs::path branch = git_dir / "refs" / "heads" / target;
    if (fs::exists(branch))
        return trim(read_text(branch));

    // 2. Tag ref?
    const fs::path tag = git_dir / "refs" / "tags" / target;
    if (fs::exists(tag))
    {
        const std::string tag_content = trim(read_text(tag));
        // Check if this tag points to an annotated tag object
        try
        {
            ObjectDatabase db(git_dir / "objects");
            const std::string raw = db.read(tag_content);
            if (raw.rfind("tag ", 0) == 0)
            {
                const size_t null_pos = raw.find('\0');
                if (null_pos != std::string::npos)
                {
                    std::istringstream iss(raw.substr(null_pos + 1));
                    std::string line;
                    while (std::getline(iss, line))
                    {
                        if (line.rfind("object ", 0) == 0)
                            return trim(line.substr(7));
                    }
                }
            }
        }
        catch (...) {}
        return tag_content;
    }

    // 3. Exact 64-char SHA?
    if (target.size() == 64)
    {
        const fs::path obj_path =
            git_dir / "objects" / target.substr(0, 2) / target.substr(2);
        if (fs::exists(obj_path))
            return target;
    }

    // 4. Short SHA prefix (>= 4 hex characters)?
    if (target.size() >= 4 && target.size() < 64)
    {
        const fs::path prefix_dir = git_dir / "objects" / target.substr(0, 2);
        if (fs::exists(prefix_dir))
        {
            const std::string rest = target.substr(2);
            std::vector<std::string> matches;
            for (const auto& entry : fs::directory_iterator(prefix_dir))
            {
                if (entry.is_regular_file())
                {
                    const std::string fname = entry.path().filename().string();
                    if (fname.rfind(rest, 0) == 0)
                        matches.push_back(target.substr(0, 2) + fname);
                }
            }
            if (matches.size() == 1)
                return matches[0];
        }
    }

    return target; // Fallback to raw target
}

// Update the ref pointed to by HEAD (or HEAD directly if detached).
static void update_ref(const fs::path& git_dir, const std::string& sha)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    fs::path ref_path;
    if (raw.size() >= 5 && raw.rfind("ref: ", 0) == 0)
    {
        ref_path = git_dir / raw.substr(5);
        fs::create_directories(ref_path.parent_path());
    }
    else
    {
        ref_path = git_dir / "HEAD";
    }

    std::ofstream f(ref_path, std::ios::trunc);
    if (!f)
        throw std::runtime_error("Could not update ref: " + ref_path.string());
    f << sha << '\n';
}

static std::string blob_body(const ObjectDatabase& db, const std::string& id)
{
    return strip_object_header(db.read(id));
}

static std::unordered_map<std::string, std::string> tree_to_map(const ParsedTree& tree)
{
    std::unordered_map<std::string, std::string> m;
    for (const auto& e : tree.entries)
        m[e.name] = e.id;
    return m;
}

static void write_file(const fs::path& abs, const std::string& content)
{
    fs::create_directories(abs.parent_path());
    std::ofstream f(abs, std::ios::binary | std::ios::trunc);
    if (!f)
        throw std::runtime_error("Cannot write file: " + abs.string());
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

// ---------------------------------------------------------------------------
// perform_cherry_pick implementation
// ---------------------------------------------------------------------------

CherryPickResult perform_cherry_pick(
    const fs::path& repo_root,
    const std::string& target,
    const std::string& author,
    bool no_commit,
    int parent_index)
{
    CherryPickResult res;
    const fs::path git_dir = repo_root / ".minigit";
    if (!fs::exists(git_dir))
    {
        res.error_message = "fatal: not a minigit repository";
        return res;
    }

    ObjectDatabase db(git_dir / "objects");

    // 1. Resolve current HEAD.
    const std::string head_sha = resolve_head(git_dir);
    if (head_sha.empty())
    {
        res.error_message = "fatal: HEAD has no commits — nothing to cherry-pick onto";
        return res;
    }

    // 2. Resolve target commit SHA.
    const std::string target_sha = resolve_commit_target(git_dir, target);
    if (target_sha.empty())
    {
        res.error_message = "fatal: ambiguous argument '" + target + "': unknown revision";
        return res;
    }

    // 3. Read and validate target commit object.
    ParsedCommit target_commit;
    try
    {
        const std::string raw = db.read(target_sha);
        if (raw.rfind("commit ", 0) != 0)
        {
            res.error_message = "fatal: '" + target_sha + "' is not a commit object";
            return res;
        }
        target_commit = parse_commit(raw);
    }
    catch (const std::exception& e)
    {
        res.error_message = "fatal: " + std::string(e.what());
        return res;
    }

    // 4. Determine base commit and base tree.
    // In cherry-pick:
    // - base: parent of target commit
    // - theirs: target commit
    // - ours: current HEAD commit
    std::unordered_map<std::string, std::string> base_map;
    if (!target_commit.parent_ids.empty())
    {
        if (target_commit.parent_ids.size() > 1)
        {
            if (parent_index < 1 || parent_index > static_cast<int>(target_commit.parent_ids.size()))
            {
                res.error_message = "error: commit " + target_sha.substr(0, 7) +
                    " is a merge but invalid -m parent number " + std::to_string(parent_index) + " was given";
                return res;
            }
        }
        const size_t p_idx = static_cast<size_t>(parent_index - 1);
        const std::string parent_sha = target_commit.parent_ids[p_idx];
        try
        {
            const ParsedCommit parent_commit = parse_commit(db.read(parent_sha));
            const ParsedTree parent_tree = parse_tree(db.read(parent_commit.tree_id));
            base_map = tree_to_map(parent_tree);
        }
        catch (const std::exception& e)
        {
            res.error_message = "fatal: could not read parent commit: " + std::string(e.what());
            return res;
        }
    }
    // If parent_ids is empty, target is a root commit; base_map remains empty.

    // 5. Build theirs (target commit) tree.
    std::unordered_map<std::string, std::string> theirs_map;
    try
    {
        const ParsedTree target_tree = parse_tree(db.read(target_commit.tree_id));
        theirs_map = tree_to_map(target_tree);
    }
    catch (const std::exception& e)
    {
        res.error_message = "fatal: could not read target tree: " + std::string(e.what());
        return res;
    }

    // 6. Build ours (HEAD) tree.
    std::unordered_map<std::string, std::string> ours_map;
    try
    {
        const ParsedCommit head_commit = parse_commit(db.read(head_sha));
        const ParsedTree head_tree = parse_tree(db.read(head_commit.tree_id));
        ours_map = tree_to_map(head_tree);
    }
    catch (const std::exception& e)
    {
        res.error_message = "fatal: could not read HEAD tree: " + std::string(e.what());
        return res;
    }

    // 7. Labels for conflict markers.
    const std::string branch_name = current_branch_name(git_dir);
    const std::string ours_label = (branch_name == "HEAD") ? "HEAD" : branch_name;
    const std::string theirs_label = target_sha.substr(0, 7) + "... " + first_line(target_commit.message);

    // 8. Three-way merge across all paths.
    std::unordered_set<std::string> all_paths;
    for (const auto& [p, _] : ours_map)   all_paths.insert(p);
    for (const auto& [p, _] : theirs_map) all_paths.insert(p);
    for (const auto& [p, _] : base_map)   all_paths.insert(p);

    std::unordered_map<std::string, std::string> result_index;
    bool any_conflict = false;

    for (const auto& path : all_paths)
    {
        const auto ours_it   = ours_map.find(path);
        const auto theirs_it = theirs_map.find(path);
        const auto base_it   = base_map.find(path);

        const bool in_ours   = (ours_it   != ours_map.end());
        const bool in_theirs = (theirs_it != theirs_map.end());
        const bool in_base   = (base_it   != base_map.end());

        // 1) Identical in both ours and theirs
        if (in_ours && in_theirs && ours_it->second == theirs_it->second)
        {
            result_index[path] = ours_it->second;
            continue;
        }

        // 2) Target didn't modify it relative to base
        if (in_base && in_theirs && base_it->second == theirs_it->second)
        {
            if (in_ours)
                result_index[path] = ours_it->second;
            continue;
        }

        // 3) File only in ours
        if (in_ours && !in_theirs)
        {
            if (!in_base)
            {
                // Added only in HEAD; target commit never touched it
                result_index[path] = ours_it->second;
            }
            else if (ours_it->second == base_it->second)
            {
                // Unchanged in ours, deleted in target -> accept deletion
                std::error_code ec;
                fs::remove(repo_root / path, ec);
            }
            else
            {
                // Modified in ours, deleted in target -> modify/delete conflict
                result_index[path] = ours_it->second;
                res.conflicted_files.push_back(path);
                any_conflict = true;
            }
            continue;
        }

        // 4) File only in theirs
        if (!in_ours && in_theirs)
        {
            if (!in_base)
            {
                // Added in target -> accept addition
                const std::string body = blob_body(db, theirs_it->second);
                write_file(repo_root / path, body);
                result_index[path] = theirs_it->second;
            }
            else if (theirs_it->second == base_it->second)
            {
                // Unchanged in target, deleted in ours -> keep deleted
            }
            else
            {
                // Modified in target, deleted in ours -> delete/modify conflict
                const std::string body = blob_body(db, theirs_it->second);
                write_file(repo_root / path, body);
                result_index[path] = theirs_it->second;
                res.conflicted_files.push_back(path);
                any_conflict = true;
            }
            continue;
        }

        // 5) Both have the file with differing contents
        if (in_ours && in_theirs)
        {
            if (in_base && ours_it->second == base_it->second)
            {
                // Ours unchanged, target modified -> cleanly accept target's modification
                const std::string body = blob_body(db, theirs_it->second);
                write_file(repo_root / path, body);
                result_index[path] = theirs_it->second;
            }
            else
            {
                // Both modified (or neither in base) -> three-way line merge
                const std::string base_content = in_base ? blob_body(db, base_it->second) : std::string{};
                const std::string ours_content = blob_body(db, ours_it->second);
                const std::string theirs_content = blob_body(db, theirs_it->second);

                FileMergeResult merge_result = three_way_merge(
                    base_content, ours_content, theirs_content,
                    ours_label, theirs_label
                );

                write_file(repo_root / path, merge_result.content);

                Blob result_blob(merge_result.content);
                db.write(result_blob.id(), result_blob.serialized());
                result_index[path] = result_blob.id();

                if (merge_result.status == MergeStatus::Conflict)
                {
                    res.conflicted_files.push_back(path);
                    any_conflict = true;
                }
            }
            continue;
        }
    }

    // 9. Rebuild the index with the resulting state.
    {
        std::ofstream clr(git_dir / "index", std::ios::trunc);
    }
    Index new_index(git_dir / "index");
    for (const auto& [path, blob_id] : result_index)
        new_index.add(path, blob_id);
    new_index.write();

    if (any_conflict)
    {
        res.conflict = true;
        res.success = false;
        res.error_message = "error: could not apply " + target_sha.substr(0, 7) +
            "... " + first_line(target_commit.message);
        return res;
    }

    // 10. If --no-commit was requested, stop here.
    if (no_commit)
    {
        res.success = true;
        res.conflict = false;
        return res;
    }

    // 11. Write the new tree from the merged result.
    std::vector<TreeEntry> tree_entries;
    tree_entries.reserve(result_index.size());
    for (const auto& [path, blob_id] : result_index)
        tree_entries.push_back({"100644", path, blob_id});

    Tree cherry_tree(std::move(tree_entries));
    db.write(cherry_tree.id(), cherry_tree.serialized());

    // 12. Create the commit object.
    const std::string commit_author = author.empty() ? target_commit.author : author;
    Commit cherry_commit(
        cherry_tree.id(),
        {head_sha},
        commit_author,
        target_commit.message
    );
    db.write(cherry_commit.id(), cherry_commit.serialized());

    // 13. Advance HEAD reference.
    update_ref(git_dir, cherry_commit.id());

    res.success = true;
    res.conflict = false;
    res.new_commit_sha = cherry_commit.id();
    return res;
}

// ---------------------------------------------------------------------------
// CLI cherry_pick_command
// ---------------------------------------------------------------------------

void cherry_pick_command(
    const std::string& target,
    const std::string& author,
    bool no_commit,
    int parent_index)
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    CherryPickResult result = perform_cherry_pick(
        repo.root(), target, author, no_commit, parent_index
    );

    if (result.conflict)
    {
        for (const auto& file : result.conflicted_files)
        {
            std::cerr << "CONFLICT (content): Merge conflict in " << file << '\n';
        }
        std::cerr << result.error_message << '\n';
        std::cerr << "hint: after resolving the conflicts, mark the corrected paths\n";
        std::cerr << "hint: with 'minigit add <paths>' or 'minigit rm <paths>'\n";
        std::cerr << "hint: and commit the result with 'minigit commit'\n";
        std::exit(1);
    }

    if (!result.success)
    {
        std::cerr << result.error_message << '\n';
        std::exit(1);
    }

    if (no_commit)
    {
        std::cout << "Cherry-pick applied staged changes; working tree and index updated (no commit created).\n";
        return;
    }

    const std::string branch = current_branch_name(repo.git_dir());
    // Read commit message title
    std::string commit_title;
    try
    {
        ObjectDatabase db(repo.git_dir() / "objects");
        const ParsedCommit c = parse_commit(db.read(result.new_commit_sha));
        commit_title = first_line(c.message);
    }
    catch (...)
    {
        commit_title = "cherry-pick";
    }

    std::cout << "[" << branch << " " << result.new_commit_sha.substr(0, 7) << "] "
              << commit_title << '\n';
}
