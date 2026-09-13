#pragma once

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Merge result for a single file.
// ---------------------------------------------------------------------------

enum class MergeStatus
{
    Clean,    // Merged without conflicts.
    Conflict, // Conflict markers were inserted.
    Identical // Both sides are the same — nothing to do.
};

struct FileMergeResult
{
    MergeStatus status;
    std::string content; // Final merged content (may contain conflict markers).
};

// ---------------------------------------------------------------------------
// Three-way line merge.
//
// Given three versions of the same file (base, ours, theirs), produces a
// merged result.  Where both sides made the same change relative to base,
// the change is accepted cleanly.  Where the two sides made *different*
// changes, conflict markers are inserted:
//
//   <<<<<<< ours
//   ... our lines ...
//   =======
//   ... their lines ...
//   >>>>>>> theirs
//
// `ours_label` and `theirs_label` are used in the conflict markers (typically
// the branch names being merged).
// ---------------------------------------------------------------------------
FileMergeResult three_way_merge(
    const std::string& base,
    const std::string& ours,
    const std::string& theirs,
    const std::string& ours_label   = "ours",
    const std::string& theirs_label = "theirs"
);

// ---------------------------------------------------------------------------
// Lowest Common Ancestor (merge-base) computation.
//
// Walks the commit DAG via BFS/BFS colouring to find the most recent commit
// that is an ancestor of both `sha_a` and `sha_b`.  Returns the commit SHA,
// or an empty string when no common ancestor exists (disconnected histories).
//
// `read_object` is a callable(sha) -> raw_object_bytes that the engine uses
// to fetch commit objects; it must throw on missing objects.
// ---------------------------------------------------------------------------
std::string find_merge_base(
    const std::string& sha_a,
    const std::string& sha_b,
    const std::function<std::string(const std::string&)>& read_object
);
