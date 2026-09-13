#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

// ---------------------------------------------------------------------------
// Object transfer helpers shared by fetch, push, and clone.
// ---------------------------------------------------------------------------

namespace transfer
{
    // Walk the commit DAG rooted at `commit_sha` in `src_objects_dir` and
    // collect every reachable object SHA that is missing from
    // `dst_objects_dir`.  Returns the set of SHAs to transfer.
    std::unordered_set<std::string>
    missing_objects(const std::filesystem::path &src_objects_dir,
                    const std::filesystem::path &dst_objects_dir,
                    const std::string           &commit_sha);

    // Copy a single loose object file from src to dst (no-op if already
    // present in dst).
    void copy_object(const std::filesystem::path &src_objects_dir,
                     const std::filesystem::path &dst_objects_dir,
                     const std::string           &sha);

    // Transfer every object in `shas` from src to dst.
    void transfer_objects(const std::filesystem::path         &src_objects_dir,
                          const std::filesystem::path         &dst_objects_dir,
                          const std::unordered_set<std::string> &shas);

    // Return true when an ancestor chain starting from `ancestor` eventually
    // reaches `descendant` in the given object store.  Used to verify
    // fast-forward eligibility.
    bool is_ancestor(const std::filesystem::path &objects_dir,
                     const std::string           &ancestor,
                     const std::string           &descendant);
}
