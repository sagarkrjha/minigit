#include "rebase.h"

#include "cherry_pick.h"
#include "merge_engine.h"
#include "repository/repository.h"
#include "staging/index.h"
#include "storage/blob.h"
#include "storage/commit.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"
#include "storage/tree.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
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

static void write_text(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
        throw std::runtime_error("Could not write to " + path.string());
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
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

static bool is_all_hex(const std::string& s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
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

// Determine active branch ref name (e.g. "refs/heads/feature" or "HEAD" if detached).
static std::string current_branch_ref(const fs::path& git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.rfind("ref: ", 0) == 0)
        return raw.substr(5);
    return "HEAD";
}

// Determine active branch short name (or "HEAD" if detached).
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

// Build a flat path -> blobId map from a parsed tree.
static std::unordered_map<std::string, std::string> tree_to_map(const ParsedTree& tree)
{
    std::unordered_map<std::string, std::string> m;
    for (const auto& e : tree.entries)
        m[e.name] = e.id;
    return m;
}

// Resolve base reference (branch, tag, full SHA, short SHA) without ancestry modifiers.
static std::string resolve_base_ref(const fs::path& git_dir, ObjectDatabase& db, const std::string& target)
{
    if (target.empty() || target == "HEAD")
        return resolve_head(git_dir);

    // 1. Branch ref
    const fs::path branch = git_dir / "refs" / "heads" / target;
    if (fs::exists(branch))
        return trim(read_text(branch));

    // 2. Tag ref
    const fs::path tag = git_dir / "refs" / "tags" / target;
    if (fs::exists(tag))
    {
        const std::string tag_content = trim(read_text(tag));
        try
        {
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

    // 3. Remote tracking ref
    const fs::path remote_ref = git_dir / "refs" / "remotes" / target;
    if (fs::exists(remote_ref))
        return trim(read_text(remote_ref));

    // 4. Relative to git_dir
    const fs::path direct_ref = git_dir / target;
    if (fs::exists(direct_ref) && !fs::is_directory(direct_ref))
        return trim(read_text(direct_ref));

    // 5. Exact 64-char SHA
    if (target.size() == 64 && is_all_hex(target))
    {
        const fs::path obj_path = git_dir / "objects" / target.substr(0, 2) / target.substr(2);
        if (fs::exists(obj_path))
            return target;
    }

    // 6. Short SHA prefix (>= 4 hex chars)
    if (target.size() >= 4 && target.size() < 64 && is_all_hex(target))
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

    return {};
}

// Comprehensive commit target resolution with ~N and ^ support.
static std::string resolve_commit_target(const fs::path& git_dir, ObjectDatabase& db, const std::string& raw_target)
{
    std::string base_name = raw_target;
    int ancestor_count = 0;

    const auto tilde_pos = raw_target.find('~');
    const auto caret_pos = raw_target.find('^');

    if (tilde_pos != std::string::npos)
    {
        base_name = raw_target.substr(0, tilde_pos);
        const std::string num_str = raw_target.substr(tilde_pos + 1);
        ancestor_count = num_str.empty() ? 1 : std::max(0, std::stoi(num_str));
    }
    else if (caret_pos != std::string::npos)
    {
        base_name = raw_target.substr(0, caret_pos);
        const std::string num_str = raw_target.substr(caret_pos + 1);
        ancestor_count = num_str.empty() ? 1 : std::max(0, std::stoi(num_str));
    }

    std::string current_sha = resolve_base_ref(git_dir, db, base_name);
    if (current_sha.empty())
        return {};

    for (int i = 0; i < ancestor_count; ++i)
    {
        try
        {
            ParsedCommit c = parse_commit(db.read(current_sha));
            if (c.parent_ids.empty())
                return {};
            current_sha = c.parent_ids[0];
        }
        catch (...)
        {
            return {};
        }
    }

    return current_sha;
}

// Restore files and index to exactly match target_tree, deleting untracked or stale files.
static void restore_working_tree_and_index(
    const fs::path& repo_root,
    const fs::path& git_dir,
    ObjectDatabase& db,
    const ParsedTree& target_tree)
{
    // Read existing index entries to know what files were tracked
    Index old_index(git_dir / "index");
    std::unordered_set<std::string> new_names;
    for (const auto& entry : target_tree.entries)
        new_names.insert(entry.name);

    // Remove files that were tracked but are no longer in target_tree
    for (const auto& [path, _] : old_index.entries())
    {
        if (!new_names.count(path))
        {
            std::error_code ec;
            fs::remove(repo_root / path, ec);
        }
    }

    // Clear existing index
    {
        std::ofstream clear(git_dir / "index", std::ios::trunc);
    }
    Index fresh_index(git_dir / "index");

    // Write all files from target_tree to working tree and populate fresh index
    for (const auto& entry : target_tree.entries)
    {
        const std::string body = strip_object_header(db.read(entry.id));
        const fs::path abs = repo_root / entry.name;
        fs::create_directories(abs.parent_path());
        std::ofstream f(abs, std::ios::binary | std::ios::trunc);
        if (!f)
            throw std::runtime_error("Cannot write file: " + abs.string());
        f.write(body.data(), static_cast<std::streamsize>(body.size()));
        fresh_index.add(entry.name, entry.id);
    }
    fresh_index.write();
}

// Verify working tree and index cleanliness before rebase.
static bool check_working_tree_clean(
    const fs::path& repo_root,
    const fs::path& git_dir,
    const std::string& head_sha,
    ObjectDatabase& db,
    std::string& dirty_reason)
{
    Index index(git_dir / "index");

    // 1. Check staged changes (index vs HEAD commit tree)
    if (!head_sha.empty())
    {
        try
        {
            ParsedCommit head_c = parse_commit(db.read(head_sha));
            ParsedTree head_t = parse_tree(db.read(head_c.tree_id));
            auto head_map = tree_to_map(head_t);

            const auto& idx_entries = index.entries();
            if (idx_entries.size() != head_map.size())
            {
                dirty_reason = "You have staged changes.";
                return false;
            }
            for (const auto& [p, blob_id] : idx_entries)
            {
                auto it = head_map.find(p);
                if (it == head_map.end() || it->second != blob_id)
                {
                    dirty_reason = "You have staged changes.";
                    return false;
                }
            }
        }
        catch (...) {}
    }

    // 2. Check unstaged changes (working tree vs index)
    for (const auto& [p, blob_id] : index.entries())
    {
        const fs::path abs = repo_root / p;
        if (!fs::exists(abs))
        {
            dirty_reason = "You have unstaged changes (missing: " + p + ").";
            return false;
        }
        std::ifstream f(abs, std::ios::binary);
        if (!f)
        {
            dirty_reason = "You have unstaged changes (" + p + ").";
            return false;
        }
        std::string content{std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
        Blob b(std::move(content));
        if (b.id() != blob_id)
        {
            dirty_reason = "You have unstaged changes (" + p + ").";
            return false;
        }
    }

    return true;
}

// Check if any tracked files contain conflict markers.
static bool has_conflict_markers(const fs::path& repo_root, const fs::path& git_dir)
{
    Index index(git_dir / "index");
    for (const auto& [path, _] : index.entries())
    {
        const fs::path abs = repo_root / path;
        if (!fs::exists(abs)) continue;
        std::ifstream f(abs, std::ios::binary);
        if (!f) continue;
        std::string line;
        while (std::getline(f, line))
        {
            if (line.rfind("<<<<<<<", 0) == 0 ||
                line == "=======" || line == "=======\r" ||
                line.rfind(">>>>>>>", 0) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

// Collect commits in range (base_sha..head_sha] in chronological replay order.
static std::vector<std::string> collect_commits_to_replay(
    ObjectDatabase& db,
    const std::string& head_sha,
    const std::string& upstream_sha,
    const std::string& base_sha)
{
    // Find all ancestors of upstream
    std::unordered_set<std::string> upstream_ancestors;
    std::queue<std::string> q;
    q.push(upstream_sha);
    while (!q.empty())
    {
        std::string sha = q.front();
        q.pop();
        if (sha.empty() || upstream_ancestors.count(sha))
            continue;
        upstream_ancestors.insert(sha);
        try
        {
            ParsedCommit c = parse_commit(db.read(sha));
            for (const auto& p : c.parent_ids)
                q.push(p);
        }
        catch (...) {}
    }

    // Walk backwards from head_sha until base_sha or an upstream ancestor
    std::vector<std::string> rev_commits;
    std::string curr = head_sha;
    std::unordered_set<std::string> visited;

    while (!curr.empty() && curr != base_sha && !upstream_ancestors.count(curr))
    {
        if (visited.count(curr)) break;
        visited.insert(curr);
        rev_commits.push_back(curr);

        try
        {
            ParsedCommit c = parse_commit(db.read(curr));
            if (c.parent_ids.empty())
                break;
            curr = c.parent_ids[0];
        }
        catch (...)
        {
            break;
        }
    }

    std::vector<std::string> result = rev_commits;
    std::reverse(result.begin(), result.end());
    return result;
}

// Sequential cherry-pick loop for rebase replay.
static RebaseResult replay_loop(
    const fs::path& repo_root,
    const fs::path& git_dir,
    ObjectDatabase& db,
    const std::string& head_name,
    const std::string& orig_head,
    const std::string& onto_sha,
    const std::vector<std::string>& commits_to_replay,
    size_t start_idx = 0)
{
    RebaseResult res;
    const fs::path rebase_dir = git_dir / "rebase-apply";

    for (size_t i = start_idx; i < commits_to_replay.size(); ++i)
    {
        const std::string& commit_sha = commits_to_replay[i];
        ParsedCommit c;
        try
        {
            c = parse_commit(db.read(commit_sha));
        }
        catch (const std::exception& e)
        {
            res.error_message = "fatal: could not read commit " + commit_sha + ": " + e.what();
            return res;
        }

        const std::string msg_first = first_line(c.message);
        std::cout << "Applying: " << msg_first << '\n';

        CherryPickResult cp = perform_cherry_pick(repo_root, commit_sha, c.author);

        if (cp.conflict)
        {
            res.conflict = true;
            res.conflicted_files = cp.conflicted_files;
            res.error_message = "error: could not apply " + commit_sha.substr(0, 7) + "... " + msg_first;

            // Persist rebase state for --continue / --abort / --skip
            fs::create_directories(rebase_dir);
            write_text(rebase_dir / "head-name", head_name + "\n");
            write_text(rebase_dir / "orig-head", orig_head + "\n");
            write_text(rebase_dir / "onto", onto_sha + "\n");
            write_text(rebase_dir / "current", commit_sha + "\n");
            write_text(rebase_dir / "current-author", c.author + "\n");
            write_text(rebase_dir / "current-message", c.message + "\n");

            std::ostringstream todo_ss;
            for (size_t j = i + 1; j < commits_to_replay.size(); ++j)
                todo_ss << commits_to_replay[j] << '\n';
            write_text(rebase_dir / "todo", todo_ss.str());

            return res;
        }

        if (!cp.success)
        {
            res.error_message = cp.error_message;
            return res;
        }
    }

    // Rebase completed successfully!
    const std::string final_head = resolve_head(git_dir);
    if (head_name.rfind("refs/heads/", 0) == 0)
    {
        const fs::path branch_ref_path = git_dir / head_name;
        fs::create_directories(branch_ref_path.parent_path());
        write_text(branch_ref_path, final_head + "\n");
        write_text(git_dir / "HEAD", "ref: " + head_name + "\n");
        std::cout << "Successfully rebased and updated " << head_name << ".\n";
    }
    else
    {
        write_text(git_dir / "HEAD", final_head + "\n");
        std::cout << "Successfully rebased and updated HEAD.\n";
    }

    std::error_code ec;
    fs::remove_all(rebase_dir, ec);

    res.success = true;
    res.new_head_sha = final_head;
    return res;
}

// ---------------------------------------------------------------------------
// perform_rebase implementation
// ---------------------------------------------------------------------------

RebaseResult perform_rebase(
    const fs::path& repo_root,
    const std::string& upstream,
    const std::string& onto)
{
    RebaseResult res;
    const fs::path git_dir = repo_root / ".minigit";
    if (!fs::exists(git_dir))
    {
        res.error_message = "fatal: not a minigit repository";
        return res;
    }

    const fs::path rebase_dir = git_dir / "rebase-apply";
    if (fs::exists(rebase_dir))
    {
        res.error_message = "fatal: It seems that there is already a rebase-apply directory, and\n"
                            "I wonder if you are in the middle of another rebase.  If that is the\n"
                            "case, please decide whether you want to proceed with:\n\n"
                            "    minigit rebase --continue\n"
                            "    minigit rebase --skip\n"
                            "    minigit rebase --abort";
        return res;
    }

    ObjectDatabase db(git_dir / "objects");

    // 1. Resolve HEAD
    const std::string head_sha = resolve_head(git_dir);
    if (head_sha.empty())
    {
        res.error_message = "fatal: HEAD has no commits — cannot rebase.";
        return res;
    }

    // 2. Resolve upstream target
    const std::string upstream_sha = resolve_commit_target(git_dir, db, upstream);
    if (upstream_sha.empty())
    {
        res.error_message = "fatal: invalid upstream '" + upstream + "'";
        return res;
    }

    // 3. Resolve onto target
    const std::string onto_target = onto.empty() ? upstream : onto;
    const std::string onto_sha = resolve_commit_target(git_dir, db, onto_target);
    if (onto_sha.empty())
    {
        res.error_message = "fatal: invalid onto target '" + onto_target + "'";
        return res;
    }

    // 4. Verify clean working tree & index
    std::string dirty_reason;
    if (!check_working_tree_clean(repo_root, git_dir, head_sha, db, dirty_reason))
    {
        res.error_message = "error: cannot rebase: " + dirty_reason + "\nerror: Please commit or stash them.";
        return res;
    }

    const std::string head_ref = current_branch_ref(git_dir);
    const std::string branch_name = current_branch_name(git_dir);

    // 5. Up-to-date check if identical
    if (head_sha == upstream_sha && onto_sha == upstream_sha)
    {
        res.success = true;
        res.up_to_date = true;
        res.new_head_sha = head_sha;
        std::cout << "Current branch " << branch_name << " is up to date.\n";
        return res;
    }

    // 6. Find merge base
    const std::string base_sha = find_merge_base(
        head_sha, upstream_sha,
        [&](const std::string& sha) { return db.read(sha); }
    );

    if (base_sha.empty())
    {
        res.error_message = "fatal: refusing to rebase unrelated histories (no common ancestor found)";
        return res;
    }

    // 7. Check if upstream is already ancestor of HEAD (up to date)
    if (base_sha == upstream_sha && onto_sha == upstream_sha)
    {
        res.success = true;
        res.up_to_date = true;
        res.new_head_sha = head_sha;
        std::cout << "Current branch " << branch_name << " is up to date.\n";
        return res;
    }

    // 8. Fast-forward check: if HEAD is ancestor of onto
    if (base_sha == head_sha && onto_sha == upstream_sha)
    {
        try
        {
            ParsedCommit onto_c = parse_commit(db.read(onto_sha));
            ParsedTree onto_t = parse_tree(db.read(onto_c.tree_id));
            restore_working_tree_and_index(repo_root, git_dir, db, onto_t);
            if (head_ref.rfind("refs/heads/", 0) == 0)
            {
                write_text(git_dir / head_ref, onto_sha + "\n");
                write_text(git_dir / "HEAD", "ref: " + head_ref + "\n");
                std::cout << "Successfully rebased and updated " << head_ref << ".\n";
            }
            else
            {
                write_text(git_dir / "HEAD", onto_sha + "\n");
                std::cout << "Successfully rebased and updated HEAD.\n";
            }
            res.success = true;
            res.fast_forward = true;
            res.new_head_sha = onto_sha;
            return res;
        }
        catch (const std::exception& e)
        {
            res.error_message = "fatal: fast-forward failed: " + std::string(e.what());
            return res;
        }
    }

    // 9. Collect commits to replay
    std::vector<std::string> commits_to_replay = collect_commits_to_replay(
        db, head_sha, upstream_sha, base_sha
    );

    if (commits_to_replay.empty())
    {
        res.success = true;
        res.up_to_date = true;
        res.new_head_sha = head_sha;
        std::cout << "Current branch " << branch_name << " is up to date.\n";
        return res;
    }

    // 10. Rewind working tree and index to onto_sha, detaching HEAD
    try
    {
        ParsedCommit onto_c = parse_commit(db.read(onto_sha));
        ParsedTree onto_t = parse_tree(db.read(onto_c.tree_id));
        restore_working_tree_and_index(repo_root, git_dir, db, onto_t);
        write_text(git_dir / "HEAD", onto_sha + "\n");
    }
    catch (const std::exception& e)
    {
        res.error_message = "fatal: could not checkout onto commit: " + std::string(e.what());
        return res;
    }

    std::cout << "First, rewinding head to replay your work on top of it...\n";

    // 11. Run replay loop
    return replay_loop(repo_root, git_dir, db, head_ref, head_sha, onto_sha, commits_to_replay, 0);
}

// ---------------------------------------------------------------------------
// rebase_continue implementation
// ---------------------------------------------------------------------------

RebaseResult rebase_continue(const fs::path& repo_root)
{
    RebaseResult res;
    const fs::path git_dir = repo_root / ".minigit";
    const fs::path rebase_dir = git_dir / "rebase-apply";

    if (!fs::exists(rebase_dir))
    {
        res.error_message = "fatal: No rebase in progress?";
        return res;
    }

    ObjectDatabase db(git_dir / "objects");

    // 1. Check for remaining conflict markers
    if (has_conflict_markers(repo_root, git_dir))
    {
        res.conflict = true;
        res.error_message = "error: Committing is not possible because you have unmerged files.\n"
                            "hint: Fix them up in the work tree, and then use 'minigit add <file>'\n"
                            "fatal: Exiting because of an unresolved conflict.";
        return res;
    }

    // 2. Read saved state
    const std::string head_name = trim(read_text(rebase_dir / "head-name"));
    const std::string orig_head = trim(read_text(rebase_dir / "orig-head"));
    const std::string onto_sha  = trim(read_text(rebase_dir / "onto"));
    const std::string author    = trim(read_text(rebase_dir / "current-author"));
    const std::string message   = read_text(rebase_dir / "current-message");

    // 3. Build tree from current index
    Index idx(git_dir / "index");
    std::vector<TreeEntry> entries;
    for (const auto& [name, blob_id] : idx.entries())
        entries.push_back({"100644", name, blob_id});

    Tree new_tree(std::move(entries));
    db.write(new_tree.id(), new_tree.serialized());

    const std::string curr_head = resolve_head(git_dir);
    ParsedCommit head_commit = parse_commit(db.read(curr_head));

    // 4. Create commit if tree changed
    if (new_tree.id() != head_commit.tree_id)
    {
        Commit new_commit(
            new_tree.id(),
            {curr_head},
            author,
            message
        );
        db.write(new_commit.id(), new_commit.serialized());
        write_text(git_dir / "HEAD", new_commit.id() + "\n");
    }
    else
    {
        std::cout << "No changes - commit skipped.\n";
    }

    // 5. Read remaining commits from todo
    std::vector<std::string> remaining;
    {
        std::ifstream todo_file(rebase_dir / "todo");
        std::string line;
        while (std::getline(todo_file, line))
        {
            line = trim(line);
            if (!line.empty())
                remaining.push_back(line);
        }
    }

    if (remaining.empty())
    {
        // Rebase complete!
        const std::string final_head = resolve_head(git_dir);
        if (head_name.rfind("refs/heads/", 0) == 0)
        {
            const fs::path branch_ref_path = git_dir / head_name;
            fs::create_directories(branch_ref_path.parent_path());
            write_text(branch_ref_path, final_head + "\n");
            write_text(git_dir / "HEAD", "ref: " + head_name + "\n");
            std::cout << "Successfully rebased and updated " << head_name << ".\n";
        }
        else
        {
            write_text(git_dir / "HEAD", final_head + "\n");
            std::cout << "Successfully rebased and updated HEAD.\n";
        }
        std::error_code ec;
        fs::remove_all(rebase_dir, ec);

        res.success = true;
        res.new_head_sha = final_head;
        return res;
    }

    // 6. Continue replay loop
    return replay_loop(repo_root, git_dir, db, head_name, orig_head, onto_sha, remaining, 0);
}

// ---------------------------------------------------------------------------
// rebase_abort implementation
// ---------------------------------------------------------------------------

RebaseResult rebase_abort(const fs::path& repo_root)
{
    RebaseResult res;
    const fs::path git_dir = repo_root / ".minigit";
    const fs::path rebase_dir = git_dir / "rebase-apply";

    if (!fs::exists(rebase_dir))
    {
        res.error_message = "fatal: No rebase in progress?";
        return res;
    }

    ObjectDatabase db(git_dir / "objects");

    const std::string head_name = trim(read_text(rebase_dir / "head-name"));
    const std::string orig_head = trim(read_text(rebase_dir / "orig-head"));

    try
    {
        ParsedCommit orig_c = parse_commit(db.read(orig_head));
        ParsedTree orig_t = parse_tree(db.read(orig_c.tree_id));
        restore_working_tree_and_index(repo_root, git_dir, db, orig_t);

        if (head_name.rfind("refs/heads/", 0) == 0)
        {
            const fs::path branch_ref_path = git_dir / head_name;
            fs::create_directories(branch_ref_path.parent_path());
            write_text(branch_ref_path, orig_head + "\n");
            write_text(git_dir / "HEAD", "ref: " + head_name + "\n");
        }
        else
        {
            write_text(git_dir / "HEAD", orig_head + "\n");
        }

        std::error_code ec;
        fs::remove_all(rebase_dir, ec);

        res.success = true;
        res.aborted = true;
        res.new_head_sha = orig_head;
        return res;
    }
    catch (const std::exception& e)
    {
        res.error_message = "fatal: could not abort rebase: " + std::string(e.what());
        return res;
    }
}

// ---------------------------------------------------------------------------
// rebase_skip implementation
// ---------------------------------------------------------------------------

RebaseResult rebase_skip(const fs::path& repo_root)
{
    RebaseResult res;
    const fs::path git_dir = repo_root / ".minigit";
    const fs::path rebase_dir = git_dir / "rebase-apply";

    if (!fs::exists(rebase_dir))
    {
        res.error_message = "fatal: No rebase in progress?";
        return res;
    }

    ObjectDatabase db(git_dir / "objects");

    const std::string head_name = trim(read_text(rebase_dir / "head-name"));
    const std::string orig_head = trim(read_text(rebase_dir / "orig-head"));
    const std::string onto_sha  = trim(read_text(rebase_dir / "onto"));

    // 1. Reset working tree and index to current HEAD commit
    const std::string curr_head = resolve_head(git_dir);
    try
    {
        ParsedCommit head_c = parse_commit(db.read(curr_head));
        ParsedTree head_t = parse_tree(db.read(head_c.tree_id));
        restore_working_tree_and_index(repo_root, git_dir, db, head_t);
    }
    catch (const std::exception& e)
    {
        res.error_message = "fatal: could not reset to HEAD: " + std::string(e.what());
        return res;
    }

    // 2. Read remaining commits from todo
    std::vector<std::string> remaining;
    {
        std::ifstream todo_file(rebase_dir / "todo");
        std::string line;
        while (std::getline(todo_file, line))
        {
            line = trim(line);
            if (!line.empty())
                remaining.push_back(line);
        }
    }

    if (remaining.empty())
    {
        // Rebase complete!
        const std::string final_head = resolve_head(git_dir);
        if (head_name.rfind("refs/heads/", 0) == 0)
        {
            const fs::path branch_ref_path = git_dir / head_name;
            fs::create_directories(branch_ref_path.parent_path());
            write_text(branch_ref_path, final_head + "\n");
            write_text(git_dir / "HEAD", "ref: " + head_name + "\n");
            std::cout << "Successfully rebased and updated " << head_name << ".\n";
        }
        else
        {
            write_text(git_dir / "HEAD", final_head + "\n");
            std::cout << "Successfully rebased and updated HEAD.\n";
        }
        std::error_code ec;
        fs::remove_all(rebase_dir, ec);

        res.success = true;
        res.new_head_sha = final_head;
        return res;
    }

    // 3. Continue replay loop with remaining commits
    return replay_loop(repo_root, git_dir, db, head_name, orig_head, onto_sha, remaining, 0);
}

// ---------------------------------------------------------------------------
// CLI rebase_command entry point
// ---------------------------------------------------------------------------

int rebase_command(int argc, char const* argv[])
{
    std::string upstream;
    std::string onto;
    bool do_continue = false;
    bool do_abort = false;
    bool do_skip = false;
    bool interactive = false;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--continue")
        {
            do_continue = true;
        }
        else if (arg == "--abort")
        {
            do_abort = true;
        }
        else if (arg == "--skip")
        {
            do_skip = true;
        }
        else if (arg == "-i" || arg == "--interactive")
        {
            interactive = true;
        }
        else if (arg == "--onto")
        {
            if (i + 1 < argc)
            {
                onto = argv[++i];
            }
            else
            {
                std::cerr << "error: --onto requires an argument\n";
                return 1;
            }
        }
        else if (!arg.empty() && arg[0] != '-')
        {
            if (upstream.empty())
                upstream = arg;
            else
            {
                std::cerr << "error: too many arguments specified\n";
                return 1;
            }
        }
        else
        {
            std::cerr << "error: unknown option: " << arg << "\n";
            std::cerr << "usage: minigit rebase [-i] [--onto <newbase>] <upstream>\n";
            std::cerr << "   or: minigit rebase --continue\n";
            std::cerr << "   or: minigit rebase --abort\n";
            std::cerr << "   or: minigit rebase --skip\n";
            return 1;
        }
    }

    const int actions = (do_continue ? 1 : 0) + (do_abort ? 1 : 0) + (do_skip ? 1 : 0);
    if (actions > 1)
    {
        std::cerr << "error: cannot have multiple actions\n";
        return 1;
    }
    if (actions == 1 && !upstream.empty())
    {
        std::cerr << "error: cannot specify upstream when using an action flag\n";
        return 1;
    }
    if (actions == 0 && upstream.empty())
    {
        std::cerr << "usage: minigit rebase [-i] [--onto <newbase>] <upstream>\n";
        std::cerr << "   or: minigit rebase --continue\n";
        std::cerr << "   or: minigit rebase --abort\n";
        std::cerr << "   or: minigit rebase --skip\n";
        return 1;
    }

    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    RebaseResult result;

    if (do_continue)
    {
        result = rebase_continue(repo.root());
    }
    else if (do_abort)
    {
        result = rebase_abort(repo.root());
    }
    else if (do_skip)
    {
        result = rebase_skip(repo.root());
    }
    else
    {
        result = perform_rebase(repo.root(), upstream, onto);
    }

    if (result.conflict)
    {
        for (const auto& file : result.conflicted_files)
            std::cerr << "CONFLICT (content): Merge conflict in " << file << '\n';
        std::cerr << result.error_message << '\n';
        std::cerr << "hint: Resolve all conflicts manually, mark them as resolved with\n";
        std::cerr << "hint: \"minigit add <file>\", then run \"minigit rebase --continue\".\n";
        std::cerr << "hint: You can instead skip this commit: run \"minigit rebase --skip\".\n";
        std::cerr << "hint: To abort and get back to the state before \"minigit rebase\", run \"minigit rebase --abort\".\n";
        return 1;
    }

    if (!result.success)
    {
        std::cerr << result.error_message << '\n';
        return 1;
    }

    return 0;
}
