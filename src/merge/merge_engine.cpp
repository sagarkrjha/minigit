#include "merge_engine.h"

#include "diff/diff_engine.h"
#include "storage/object_parser.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ===========================================================================
// Three-way line merge implementation
// ===========================================================================

// Classify how a region from `base` was transformed in `side`.
// Each region is a contiguous run of edits from the base perspective.
// We use the LCS edit sequence to determine, for each base line, whether
// `side` kept it, deleted it, or inserted lines before/after it.

struct Region
{
    // Lines from `side` that replace (or supplement) a span of base lines.
    std::vector<std::string> lines;
    bool changed; // true if this region differs from the corresponding base span
};

// Align `side_edits` (lcs_diff(base_lines, side_lines)) into a sequence of
// regions, one per base line (plus a sentinel region at the front for any
// leading insertions).
// Returns a vector of (base_line_index -> region) where the region describes
// what `side` has in place of that base line.
// We use a simpler representation: a flat list of chunks interleaved.

// Simpler approach: represent the merged output as a sequence of "chunks".
// For each base line we record what "ours" and "theirs" produced:
//   - If both kept → emit the line.
//   - If both deleted → emit nothing.
//   - If both replaced with the same content → emit the replacement.
//   - Otherwise → conflict.
// Leading/trailing insertions (not anchored to a base line) are handled
// as independent chunks.

// We model the alignment as follows.  From `lcs_diff(base, side)` we get a
// sequence of Keep/Remove/Add edits.  We convert this into a sequence of
// "slots" aligned with base lines, where each slot contains:
//   prefix_adds : lines inserted *before* the base line (Add runs).
//   keep_or_del : true = base line kept, false = base line removed.
//   (the base line itself is implicit from the slot index)

struct Slot
{
    std::vector<std::string> prefix_adds; // inserted lines before this base line
    bool kept;                            // base line is kept in this side
    std::vector<std::string> replacement; // non-empty only if kept==false and adds follow the remove
};

// Build a slot sequence from the edit vector produced by lcs_diff(base, side).
// There is one slot per base line, plus a trailing slot for any trailing adds.
static std::vector<Slot> build_slots(const std::vector<std::string>& base_lines,
                                     const std::vector<Edit>& edits)
{
    // slots[0..base_lines.size()-1] correspond to each base line.
    // slots[base_lines.size()] is a synthetic trailing slot for trailing adds.
    const std::size_t n = base_lines.size();
    std::vector<Slot> slots(n + 1);
    for (auto& s : slots)
        s.kept = false;

    std::size_t base_idx = 0; // which base line we are currently at
    bool pending_remove = false;

    for (const auto& edit : edits)
    {
        switch (edit.type)
        {
        case EditType::Keep:
            // Any pending prefix adds have already been pushed to slots[base_idx].
            slots[base_idx].kept = true;
            ++base_idx;
            break;

        case EditType::Remove:
            // Base line base_idx is deleted in this side.
            slots[base_idx].kept = false;
            ++base_idx;
            break;

        case EditType::Add:
            // Inserted line — belongs as a prefix to the *current* base slot.
            if (base_idx < n)
                slots[base_idx].prefix_adds.push_back(edit.line);
            else
                slots[n].prefix_adds.push_back(edit.line); // trailing adds
            break;
        }
    }

    return slots;
}

FileMergeResult three_way_merge(
    const std::string& base,
    const std::string& ours,
    const std::string& theirs,
    const std::string& ours_label,
    const std::string& theirs_label)
{
    // Fast path: identical sides.
    if (ours == theirs)
        return {MergeStatus::Identical, ours};

    const auto base_lines   = split_lines(base);
    const auto ours_lines   = split_lines(ours);
    const auto theirs_lines = split_lines(theirs);

    const auto our_edits   = lcs_diff(base_lines, ours_lines);
    const auto their_edits = lcs_diff(base_lines, theirs_lines);

    const auto our_slots   = build_slots(base_lines, our_edits);
    const auto their_slots = build_slots(base_lines, their_edits);

    const std::size_t n = base_lines.size();
    std::ostringstream out;
    bool has_conflict = false;

    // Helper: emit a conflict block.
    auto emit_conflict = [&](const std::vector<std::string>& ours_chunk,
                              const std::vector<std::string>& theirs_chunk)
    {
        has_conflict = true;
        out << "<<<<<<< " << ours_label << '\n';
        for (const auto& l : ours_chunk)  out << l << '\n';
        out << "=======\n";
        for (const auto& l : theirs_chunk) out << l << '\n';
        out << ">>>>>>> " << theirs_label << '\n';
    };

    // Helper: collect what a side produces for slot `i` (prefix adds + the
    // base line if kept, or nothing if removed).
    auto collect_side = [&](const std::vector<Slot>& slots,
                             const std::vector<std::string>& base_lns,
                             std::size_t i) -> std::vector<std::string>
    {
        std::vector<std::string> result;
        for (const auto& l : slots[i].prefix_adds) result.push_back(l);
        if (i < base_lns.size() && slots[i].kept)  result.push_back(base_lns[i]);
        return result;
    };

    // Iterate over every slot (base line + trailing slot).
    for (std::size_t i = 0; i <= n; ++i)
    {
        const auto ours_chunk   = collect_side(our_slots,   base_lines, i);
        const auto theirs_chunk = collect_side(their_slots, base_lines, i);

        if (ours_chunk == theirs_chunk)
        {
            // Both sides agree — emit as-is.
            for (const auto& l : ours_chunk) out << l << '\n';
        }
        else
        {
            // Sides differ.  Check whether only one side changed.
            // "Changed" relative to what base slot i would have produced.
            std::vector<std::string> base_chunk;
            if (!our_slots[i].prefix_adds.empty() || !their_slots[i].prefix_adds.empty())
            {
                // There are adds — base had no such adds.
                // base_chunk is just the base line (if i < n) or nothing.
            }
            if (i < n) base_chunk.push_back(base_lines[i]);

            const bool ours_changed   = (ours_chunk   != base_chunk);
            const bool theirs_changed = (theirs_chunk != base_chunk);

            if (ours_changed && !theirs_changed)
            {
                // Only ours changed — accept ours.
                for (const auto& l : ours_chunk) out << l << '\n';
            }
            else if (!ours_changed && theirs_changed)
            {
                // Only theirs changed — accept theirs.
                for (const auto& l : theirs_chunk) out << l << '\n';
            }
            else
            {
                // Both changed differently — conflict.
                emit_conflict(ours_chunk, theirs_chunk);
            }
        }
    }

    const MergeStatus status = has_conflict ? MergeStatus::Conflict : MergeStatus::Clean;
    return {status, out.str()};
}

// ===========================================================================
// LCA / merge-base computation
// ===========================================================================

std::string find_merge_base(
    const std::string& sha_a,
    const std::string& sha_b,
    const std::function<std::string(const std::string&)>& read_object)
{
    // BFS from sha_a to mark all ancestors (inclusive).
    // Then BFS from sha_b; the first node encountered that is already marked
    // is the best merge base (shallowest ancestor of b that is also an
    // ancestor of a).
    //
    // This implements the "paint" algorithm used by Git for simple cases
    // (single merge-base; does not handle criss-cross merges beyond returning
    // one valid base).

    // Phase 1: collect all ancestors of sha_a into a set.
    std::unordered_set<std::string> ancestors_of_a;
    {
        std::queue<std::string> q;
        q.push(sha_a);
        while (!q.empty())
        {
            const std::string sha = q.front();
            q.pop();
            if (sha.empty() || ancestors_of_a.count(sha))
                continue;
            ancestors_of_a.insert(sha);
            try
            {
                const ParsedCommit c = parse_commit(read_object(sha));
                for (const auto& p : c.parent_ids)
                    q.push(p);
            }
            catch (...) { /* ignore unreadable objects */ }
        }
    }

    // Phase 2: BFS from sha_b; first hit in ancestors_of_a is the LCA.
    {
        std::queue<std::string> q;
        std::unordered_set<std::string> visited;
        q.push(sha_b);
        while (!q.empty())
        {
            const std::string sha = q.front();
            q.pop();
            if (sha.empty() || visited.count(sha))
                continue;
            visited.insert(sha);
            if (ancestors_of_a.count(sha))
                return sha; // found the merge base
            try
            {
                const ParsedCommit c = parse_commit(read_object(sha));
                for (const auto& p : c.parent_ids)
                    q.push(p);
            }
            catch (...) { /* ignore unreadable objects */ }
        }
    }

    return {}; // no common ancestor
}
