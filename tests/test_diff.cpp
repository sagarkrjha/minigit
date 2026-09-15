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
