#include "test_framework.h"
#include "diff/diff_engine.h"

TEST_CASE(Diff, SplitLines)
{
    const std::string text = "line 1\r\nline 2\nline 3\r\n";
    std::vector<std::string> lines = split_lines(text);

    ASSERT_EQ(lines.size(), 3);
    ASSERT_EQ(lines[0], "line 1");
    ASSERT_EQ(lines[1], "line 2");
    ASSERT_EQ(lines[2], "line 3");
}

TEST_CASE(Diff, LCSDiffEdits)
{
    std::vector<std::string> old_lines = {"apple", "banana", "cherry"};
    std::vector<std::string> new_lines = {"apple", "blueberry", "cherry"};

    std::vector<Edit> edits = lcs_diff(old_lines, new_lines);

    // Should keep apple, remove banana, add blueberry, keep cherry
    ASSERT_TRUE(edits.size() >= 4);

    bool found_remove_banana = false;
    bool found_add_blueberry = false;

    for (const auto& e : edits)
    {
        if (e.type == EditType::Remove && e.line == "banana")
            found_remove_banana = true;
        if (e.type == EditType::Add && e.line == "blueberry")
            found_add_blueberry = true;
    }

    ASSERT_TRUE(found_remove_banana);
    ASSERT_TRUE(found_add_blueberry);
}

TEST_CASE(Diff, FormatUnifiedDiff)
{
    std::vector<std::string> old_lines = {"first line", "second line"};
    std::vector<std::string> new_lines = {"first line", "modified line"};

    std::vector<Edit> edits = lcs_diff(old_lines, new_lines);
    std::string diff_output = format_unified_diff("test.txt", "test.txt", edits);

    ASSERT_TRUE(diff_output.find("--- a/test.txt") != std::string::npos);
    ASSERT_TRUE(diff_output.find("+++ b/test.txt") != std::string::npos);
    ASSERT_TRUE(diff_output.find("-second line") != std::string::npos);
    ASSERT_TRUE(diff_output.find("+modified line") != std::string::npos);
}

TEST_CASE(Diff, MyersDiffPaperExample)
{
    // Eugene Myers 1986 paper example: "ABCABBA" -> "CBABAC"
    std::vector<std::string> a = {"A", "B", "C", "A", "B", "B", "A"};
    std::vector<std::string> b = {"C", "B", "A", "B", "A", "C"};

    std::vector<Edit> edits = myers_diff(a, b);

    // Verify reconstruction of b from a using edit script
    std::vector<std::string> reconstructed;
    size_t a_idx = 0;
    for (const auto& e : edits)
    {
        if (e.type == EditType::Keep)
        {
            ASSERT_TRUE(a_idx < a.size() && a[a_idx] == e.line);
            reconstructed.push_back(e.line);
            a_idx++;
        }
        else if (e.type == EditType::Remove)
        {
            ASSERT_TRUE(a_idx < a.size() && a[a_idx] == e.line);
            a_idx++;
        }
        else if (e.type == EditType::Add)
        {
            reconstructed.push_back(e.line);
        }
    }
    ASSERT_EQ(a_idx, a.size());
    ASSERT_EQ(reconstructed.size(), b.size());
    for (size_t i = 0; i < b.size(); ++i)
    {
        ASSERT_EQ(reconstructed[i], b[i]);
    }
}

TEST_CASE(Diff, MyersDiffEdgeCases)
{
    // Empty inputs
    auto empty_edits = myers_diff({}, {});
    ASSERT_TRUE(empty_edits.empty());

    // Pure additions
    auto add_edits = myers_diff({}, {"one", "two"});
    ASSERT_EQ(add_edits.size(), 2);
    ASSERT_TRUE(add_edits[0].type == EditType::Add && add_edits[0].line == "one");
    ASSERT_TRUE(add_edits[1].type == EditType::Add && add_edits[1].line == "two");

    // Pure removals
    auto rem_edits = myers_diff({"one", "two"}, {});
    ASSERT_EQ(rem_edits.size(), 2);
    ASSERT_TRUE(rem_edits[0].type == EditType::Remove && rem_edits[0].line == "one");
    ASSERT_TRUE(rem_edits[1].type == EditType::Remove && rem_edits[1].line == "two");

    // Identical files
    auto id_edits = myers_diff({"x", "y"}, {"x", "y"});
    ASSERT_EQ(id_edits.size(), 2);
    ASSERT_TRUE(id_edits[0].type == EditType::Keep && id_edits[0].line == "x");
    ASSERT_TRUE(id_edits[1].type == EditType::Keep && id_edits[1].line == "y");
}
