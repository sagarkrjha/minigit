#include "transfer.h"

#include "storage/object_database.h"
#include "storage/object_parser.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <queue>
#include <stdexcept>

namespace fs = std::filesystem;

namespace
{
    // Return the path of an object file in a given objects directory.
    fs::path object_path(const fs::path &objects_dir, const std::string &sha)
    {
        return objects_dir / sha.substr(0, 2) / sha.substr(2);
    }

    bool object_exists(const fs::path &objects_dir, const std::string &sha)
    {
        return fs::exists(object_path(objects_dir, sha));
    }

    // Read raw (compressed) bytes directly from the objects directory.
    std::string read_raw(const fs::path &objects_dir, const std::string &sha)
    {
        const fs::path p = object_path(objects_dir, sha);
        std::ifstream  f(p, std::ios::binary);
        if (!f)
            throw std::runtime_error("object not found: " + sha);
        return {std::istreambuf_iterator<char>(f),
                std::istreambuf_iterator<char>{}};
    }
}

namespace transfer
{
    std::unordered_set<std::string>
    missing_objects(const fs::path &src_objects_dir,
                    const fs::path &dst_objects_dir,
                    const std::string &start_sha)
    {
        std::unordered_set<std::string> needed;
        std::unordered_set<std::string> visited;
        std::queue<std::string>         queue;

        // BFS over the commit DAG starting from start_sha.
        // For each commit we also enqueue its tree and all blob objects.
        ObjectDatabase src_db(src_objects_dir);

        auto enqueue_if_missing = [&](const std::string &sha)
        {
            if (sha.empty() || visited.count(sha)) return;
            visited.insert(sha);
            if (!object_exists(dst_objects_dir, sha))
                needed.insert(sha);
        };

        // Collect all blobs referenced by a tree (flat tree — MiniGit stores
        // all files directly; no sub-trees).
        auto collect_tree = [&](const std::string &tree_sha)
        {
            enqueue_if_missing(tree_sha);
            try
            {
                const auto parsed = parse_tree(src_db.read(tree_sha));
                for (const auto &entry : parsed.entries)
                    enqueue_if_missing(entry.id);
            }
            catch (...) {} // best-effort
        };

        enqueue_if_missing(start_sha);
        queue.push(start_sha);

        while (!queue.empty())
        {
            const std::string sha = queue.front();
            queue.pop();

            ParsedCommit commit;
            try
            {
                commit = parse_commit(src_db.read(sha));
            }
            catch (...)
            {
                // Not a commit (e.g. already hit a blob/tree in the queue by
                // accident) — skip.
                continue;
            }

            collect_tree(commit.tree_id);

            for (const auto &parent : commit.parent_ids)
            {
                // Stop BFS at objects already in destination.
                if (!visited.count(parent) &&
                    !object_exists(dst_objects_dir, parent))
                {
                    enqueue_if_missing(parent);
                    queue.push(parent);
                }
                else
                {
                    visited.insert(parent); // don't cross into dst history
                }
            }
        }

        return needed;
    }

    void copy_object(const fs::path &src_objects_dir,
                     const fs::path &dst_objects_dir,
                     const std::string &sha)
    {
        const fs::path dst = object_path(dst_objects_dir, sha);
        if (fs::exists(dst)) return;

        const fs::path src = object_path(src_objects_dir, sha);
        fs::create_directories(dst.parent_path());
        fs::copy_file(src, dst, fs::copy_options::skip_existing);
    }

    void transfer_objects(const fs::path &src_objects_dir,
                          const fs::path &dst_objects_dir,
                          const std::unordered_set<std::string> &shas)
    {
        for (const auto &sha : shas)
            copy_object(src_objects_dir, dst_objects_dir, sha);
    }

    bool is_ancestor(const fs::path &objects_dir,
                     const std::string &ancestor,
                     const std::string &descendant)
    {
        if (ancestor.empty() || descendant.empty()) return false;
        if (ancestor == descendant) return true;

        ObjectDatabase db(objects_dir);
        std::unordered_set<std::string> visited;
        std::queue<std::string>         q;
        q.push(descendant);

        while (!q.empty())
        {
            const std::string cur = q.front();
            q.pop();
            if (cur == ancestor) return true;
            if (visited.count(cur)) continue;
            visited.insert(cur);

            try
            {
                const auto commit = parse_commit(db.read(cur));
                for (const auto &p : commit.parent_ids)
                    if (!visited.count(p))
                        q.push(p);
            }
            catch (...) {}
        }
        return false;
    }

} // namespace transfer
