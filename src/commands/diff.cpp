#include "diff.h"

#include "../diff/diff.h"
#include "../filesystem/file.h"
#include "../index/index.h"
#include "../objects/blob.h"
#include "../objects/object_database.h"
#include "../objects/object_parser.h"
#include "../repository/repository.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers shared with other commands (could move to a utils header later).
// ---------------------------------------------------------------------------

static std::string read_text_file(const fs::path &path)
{
    std::ifstream f(path);
    if (!f)
        return {};
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

static std::string resolve_head_sha(const fs::path &git_dir)
{
    const std::string raw = trim_trailing(read_text_file(git_dir / "HEAD"));
    if (raw.empty())
        return {};
    if (raw.substr(0, 5) == "ref: ")
        return trim_trailing(read_text_file(git_dir / raw.substr(5)));
    return raw;
}

// Read a blob from the object DB and return its content (just the body).
static std::string read_blob_content(const ObjectDatabase &db,
                                     const std::string &blob_id)
{
    const std::string raw = db.read(blob_id);
    return strip_object_header(raw);
}

// ---------------------------------------------------------------------------
// diff_command
// ---------------------------------------------------------------------------

void diff_command(bool cached, const std::vector<std::string> &paths)
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
    Index index(repo.git_dir() / "index");

    // Build a filter set if specific paths were requested.
    const bool filter = !paths.empty();

    auto should_include = [&](const std::string &path) -> bool {
        if (!filter)
            return true;
        for (const auto &p : paths)
            if (path == p || path.starts_with(p + "/"))
                return true;
        return false;
    };

    bool any_diff = false;

    if (!cached)
    {
        // ----------------------------------------------------------------
        // Mode 1: working tree vs index (unstaged changes).
        // ----------------------------------------------------------------
        for (const auto &[rel_path, blob_id] : index.entries())
        {
            if (!should_include(rel_path))
                continue;

            const fs::path abs_path = repo.root() / rel_path;

            if (!fs::exists(abs_path))
            {
                // File deleted from working tree but still staged.
                std::cout << "diff --minigit a/" << rel_path << " b/" << rel_path << '\n';
                std::cout << "--- a/" << rel_path << '\n';
                std::cout << "+++ /dev/null\n";
                const std::string old_content = read_blob_content(db, blob_id);
                for (const auto &line : split_lines(old_content))
                    std::cout << '-' << line << '\n';
                any_diff = true;
                continue;
            }

            // Read file from disk.
            std::string wt_content;
            try { wt_content = read_file(abs_path); }
            catch (...) { continue; }

            // Hash the working-tree content to see if it differs.
            Blob wt_blob(wt_content);
            if (wt_blob.id() == blob_id)
                continue; // unchanged

            // Produce unified diff.
            const std::string old_content = read_blob_content(db, blob_id);
            const auto edits = lcs_diff(split_lines(old_content),
                                        split_lines(wt_content));
            const std::string patch =
                format_unified_diff(rel_path, rel_path, edits);

            if (!patch.empty())
            {
                std::cout << patch;
                any_diff = true;
            }
        }
    }
    else
    {
        // ----------------------------------------------------------------
        // Mode 2: index vs HEAD commit (staged changes).
        // ----------------------------------------------------------------
        const std::string head_sha = resolve_head_sha(repo.git_dir());

        // Build a map of path -> blob_id from the HEAD commit's tree.
        std::unordered_map<std::string, std::string> committed; // path -> blob_id

        if (!head_sha.empty())
        {
            try
            {
                const ParsedCommit commit = parse_commit(db.read(head_sha));
                const ParsedTree   tree   = parse_tree(db.read(commit.tree_id));
                for (const auto &e : tree.entries)
                    committed[e.name] = e.id;
            }
            catch (const std::exception &e)
            {
                std::cerr << "error reading HEAD commit: " << e.what() << '\n';
                std::exit(1);
            }
        }

        // Files in index but not in HEAD, or with different blob IDs.
        for (const auto &[rel_path, blob_id] : index.entries())
        {
            if (!should_include(rel_path))
                continue;

            const auto it = committed.find(rel_path);
            const bool is_new = (it == committed.end());
            if (!is_new && it->second == blob_id)
                continue; // unchanged

            const std::string new_content = read_blob_content(db, blob_id);
            const std::string old_content =
                is_new ? "" : read_blob_content(db, it->second);

            const auto edits =
                lcs_diff(split_lines(old_content), split_lines(new_content));
            const std::string patch =
                format_unified_diff(is_new ? "/dev/null" : rel_path,
                                    rel_path, edits);

            if (!patch.empty())
            {
                std::cout << patch;
                any_diff = true;
            }
        }

        // Files in HEAD but deleted from index.
        for (const auto &[rel_path, blob_id] : committed)
        {
            if (!should_include(rel_path))
                continue;
            if (index.entries().count(rel_path))
                continue;

            const std::string old_content = read_blob_content(db, blob_id);
            const auto edits =
                lcs_diff(split_lines(old_content), {});
            const std::string patch =
                format_unified_diff(rel_path, "/dev/null", edits);

            if (!patch.empty())
            {
                std::cout << patch;
                any_diff = true;
            }
        }
    }

    if (!any_diff)
        ; // git diff exits 0 and prints nothing when clean
}
