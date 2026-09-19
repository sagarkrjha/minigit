#include "diff_engine.h"

#include <algorithm>
#include <sstream>

// ---------------------------------------------------------------------------
// split_lines
// ---------------------------------------------------------------------------

std::vector<std::string> split_lines(const std::string &text)
{
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line))
    {
        // Strip trailing \r so Windows CRLF files compare correctly.
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(std::move(line));
    }

    // If text ends with a newline, std::getline leaves an empty trailing
    // entry — drop it to avoid a spurious blank edit line.
    if (!lines.empty() && lines.back().empty() && !text.empty() &&
        text.back() == '\n')
    {
        lines.pop_back();
    }

    return lines;
}

// ---------------------------------------------------------------------------
// myers_diff  (Eugene Myers' O(ND) Greedy Difference Algorithm)
// ---------------------------------------------------------------------------

std::vector<Edit> myers_diff(const std::vector<std::string> &old_lines,
                             const std::vector<std::string> &new_lines)
{
    const int m = static_cast<int>(old_lines.size());
    const int n = static_cast<int>(new_lines.size());

    // Fast-path 1: both inputs empty
    if (m == 0 && n == 0)
        return {};

    // Fast-path 2: old is empty -> all lines are additions
    if (m == 0)
    {
        std::vector<Edit> edits;
        edits.reserve(n);
        for (const auto &line : new_lines)
            edits.push_back({EditType::Add, line});
        return edits;
    }

    // Fast-path 3: new is empty -> all lines are removals
    if (n == 0)
    {
        std::vector<Edit> edits;
        edits.reserve(m);
        for (const auto &line : old_lines)
            edits.push_back({EditType::Remove, line});
        return edits;
    }

    const int max_d = m + n;
    // v array stores furthest reaching x on diagonal k, indexed by k + max_d
    std::vector<int> v(2 * max_d + 1, 0);

    // trace[d] stores the slice of v for diagonals [-d, d] at step d.
    // Diagonal k is at trace[d][k + d].
    std::vector<std::vector<int>> trace;
    trace.reserve(max_d + 1);

    // Initial snake at d = 0
    int init_x = 0;
    while (init_x < m && init_x < n && old_lines[init_x] == new_lines[init_x])
        ++init_x;

    v[max_d] = init_x;
    trace.push_back({init_x});

    // If identical, return all Keep
    if (init_x == m && init_x == n)
    {
        std::vector<Edit> edits;
        edits.reserve(m);
        for (const auto &line : old_lines)
            edits.push_back({EditType::Keep, line});
        return edits;
    }

    bool done = false;
    for (int d = 1; d <= max_d && !done; ++d)
    {
        std::vector<int> slice(2 * d + 1, 0);

        for (int k = -d; k <= d; k += 2)
        {
            int x = 0;
            if (k == -d || (k != d && v[k - 1 + max_d] < v[k + 1 + max_d]))
            {
                x = v[k + 1 + max_d]; // Downward move (Add)
            }
            else
            {
                x = v[k - 1 + max_d] + 1; // Rightward move (Remove)
            }

            int y = x - k;

            // Snake along diagonal
            while (x < m && y < n && old_lines[x] == new_lines[y])
            {
                ++x;
                ++y;
            }

            v[k + max_d] = x;
            slice[k + d] = x;

            if (x >= m && y >= n)
            {
                done = true;
                break;
            }
        }

        trace.push_back(std::move(slice));
    }

    // Backtrack to reconstruct the edit script
    std::vector<Edit> edits;
    int x = m;
    int y = n;

    for (int d = static_cast<int>(trace.size()) - 1; d > 0; --d)
    {
        const int k = x - y;
        int prev_k = 0;

        if (k == -d || (k != d && trace[d - 1][k - 1 + (d - 1)] < trace[d - 1][k + 1 + (d - 1)]))
        {
            prev_k = k + 1;
        }
        else
        {
            prev_k = k - 1;
        }

        const int prev_x = trace[d - 1][prev_k + (d - 1)];
        const int prev_y = prev_x - prev_k;

        // Roll back diagonal snake
        while (x > prev_x && y > prev_y && x > 0 && y > 0 && old_lines[x - 1] == new_lines[y - 1])
        {
            edits.push_back({EditType::Keep, old_lines[x - 1]});
            --x;
            --y;
        }

        // Edit step
        if (prev_k < k)
        {
            // Horizontal move: Remove old_lines[prev_x]
            edits.push_back({EditType::Remove, old_lines[prev_x]});
            x = prev_x;
        }
        else
        {
            // Vertical move: Add new_lines[prev_y]
            edits.push_back({EditType::Add, new_lines[prev_y]});
            y = prev_y;
        }
    }

    // Roll back initial snake at d = 0
    while (x > 0 && y > 0 && old_lines[x - 1] == new_lines[y - 1])
    {
        edits.push_back({EditType::Keep, old_lines[x - 1]});
        --x;
        --y;
    }

    std::reverse(edits.begin(), edits.end());
    return edits;
}

std::vector<Edit> lcs_diff(const std::vector<std::string> &old_lines,
                            const std::vector<std::string> &new_lines)
{
    return myers_diff(old_lines, new_lines);
}

// ---------------------------------------------------------------------------
// format_unified_diff
// ---------------------------------------------------------------------------

std::string format_unified_diff(const std::string &old_name,
                                const std::string &new_name,
                                const std::vector<Edit> &edits,
                                int context)
{
    // --- Identify hunk ranges ---
    // A hunk is a contiguous region of changes (+/-) expanded by `context`
    // Keep lines on each side. Overlapping expansions are merged.

    const int n = static_cast<int>(edits.size());

    // Mark which edit indices have a change.
    std::vector<bool> changed(n, false);
    for (int k = 0; k < n; ++k)
        if (edits[k].type != EditType::Keep)
            changed[k] = true;

    // Build list of [start, end) ranges (inclusive of context).
    struct Range { int start, end; };
    std::vector<Range> ranges;

    for (int k = 0; k < n; ++k)
    {
        if (!changed[k])
            continue;

        // Find contiguous block of changes.
        int block_end = k;
        while (block_end + 1 < n && changed[block_end + 1])
            ++block_end;

        const int range_start = std::max(0, k - context);
        const int range_end   = std::min(n - 1, block_end + context);

        if (!ranges.empty() && range_start <= ranges.back().end + 1)
            ranges.back().end = std::max(ranges.back().end, range_end);
        else
            ranges.push_back({range_start, range_end});

        k = block_end; // skip past the block
    }

    if (ranges.empty())
        return {}; // no differences

    // --- Format output ---
    std::ostringstream out;
    out << "diff --minigit a/" << old_name << " b/" << new_name << '\n';
    out << "--- a/" << old_name << '\n';
    out << "+++ b/" << new_name << '\n';

    for (const auto &range : ranges)
    {
        // Compute old/new line numbers and counts for the @@ header.
        int old_start = 1, old_count = 0;
        int new_start = 1, new_count = 0;

        // Count Keep/Remove before range start → determines old_start.
        // Count Keep/Add before range start → determines new_start.
        int old_line = 0, new_line = 0;
        for (int k = 0; k < range.start; ++k)
        {
            if (edits[k].type != EditType::Add)    ++old_line;
            if (edits[k].type != EditType::Remove)  ++new_line;
        }
        old_start = old_line + 1;
        new_start = new_line + 1;

        for (int k = range.start; k <= range.end; ++k)
        {
            if (edits[k].type != EditType::Add)    ++old_count;
            if (edits[k].type != EditType::Remove)  ++new_count;
        }

        out << "@@ -" << old_start << ',' << old_count
            << " +" << new_start << ',' << new_count << " @@\n";

        for (int k = range.start; k <= range.end; ++k)
        {
            switch (edits[k].type)
            {
            case EditType::Keep:   out << ' ' << edits[k].line << '\n'; break;
            case EditType::Add:    out << '+' << edits[k].line << '\n'; break;
            case EditType::Remove: out << '-' << edits[k].line << '\n'; break;
            }
        }
    }

    return out.str();
}
