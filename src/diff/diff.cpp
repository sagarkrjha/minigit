#include "diff.h"

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
// lcs_diff  (O(m*n) LCS dynamic programming)
// ---------------------------------------------------------------------------

std::vector<Edit> lcs_diff(const std::vector<std::string> &old_lines,
                            const std::vector<std::string> &new_lines)
{
    const int m = static_cast<int>(old_lines.size());
    const int n = static_cast<int>(new_lines.size());

    // Build LCS table.
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
    for (int i = 1; i <= m; ++i)
        for (int j = 1; j <= n; ++j)
            dp[i][j] = (old_lines[i - 1] == new_lines[j - 1])
                           ? dp[i - 1][j - 1] + 1
                           : std::max(dp[i - 1][j], dp[i][j - 1]);

    // Backtrack iteratively to build the edit sequence in reverse.
    std::vector<Edit> edits;
    int i = m, j = n;
    while (i > 0 || j > 0)
    {
        if (i > 0 && j > 0 && old_lines[i - 1] == new_lines[j - 1])
        {
            edits.push_back({EditType::Keep, old_lines[i - 1]});
            --i; --j;
        }
        else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j]))
        {
            edits.push_back({EditType::Add, new_lines[j - 1]});
            --j;
        }
        else
        {
            edits.push_back({EditType::Remove, old_lines[i - 1]});
            --i;
        }
    }

    std::reverse(edits.begin(), edits.end());
    return edits;
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
