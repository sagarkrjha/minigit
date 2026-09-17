#include "test_framework.h"
#include "history/show.h"
#include "repository/repository.h"
#include "staging/index.h"
#include "storage/blob.h"
#include "storage/commit.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"
#include "storage/tree.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

void write_file(const fs::path& p, const std::string& content)
{
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string read_single_line(const fs::path& p)
{
    std::ifstream f(p);
    if (!f) return {};
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
        line.pop_back();
    return line;
}

fs::path create_test_repo(const std::string& subfolder)
{
    const fs::path repo_dir = fs::temp_directory_path() / "minigit_show_tests" / subfolder;
    std::error_code ec;
    fs::remove_all(repo_dir, ec);
    fs::create_directories(repo_dir);

    Repository repo(repo_dir);
    repo.init();
    return repo_dir;
}

std::string commit_files(
    const fs::path& repo_root,
    const std::vector<std::pair<std::string, std::string>>& files,
    const std::string& message,
    const std::string& author = "MiniGit Tester <tester@minigit>")
{
    const fs::path git_dir = repo_root / ".minigit";
    ObjectDatabase db(git_dir / "objects");
    Index index(git_dir / "index");

    for (const auto& [rel_path, content] : files)
    {
        write_file(repo_root / rel_path, content);
        Blob b(content);
        db.write(b.id(), b.serialized());
        index.add(rel_path, b.id());
    }

    std::vector<TreeEntry> entries;
    for (const auto& [path, id] : index.entries())
    {
        entries.push_back({"100644", path, id});
    }
    Tree tree(entries);
    db.write(tree.id(), tree.serialized());

    std::vector<std::string> parents;
    const std::string head_raw = read_single_line(git_dir / "HEAD");
    fs::path ref_path = git_dir / "HEAD";

    if (head_raw.rfind("ref: ", 0) == 0)
    {
        ref_path = git_dir / head_raw.substr(5);
        if (fs::exists(ref_path))
        {
            const std::string parent_sha = read_single_line(ref_path);
            if (!parent_sha.empty())
                parents.push_back(parent_sha);
        }
    }
    else if (!head_raw.empty())
    {
        parents.push_back(head_raw);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    Commit c(tree.id(), parents, author, message);
    db.write(c.id(), c.serialized());

    fs::create_directories(ref_path.parent_path());
    {
        std::ofstream ref_out(ref_path, std::ios::trunc);
        ref_out << c.id() << '\n';
    }

    return c.id();
}

} // namespace

TEST_CASE(Show, CommitInspectionAndDiff)
{
    const fs::path repo = create_test_repo("commit_diff");

    // Commit 1: root commit with hello.txt
    const std::string c1 = commit_files(repo, {{"hello.txt", "line 1\nline 2\n"}}, "Initial commit");

    // Inspect root commit via HEAD
    ShowResult res1 = perform_show(repo, "HEAD", ShowFormat::Default);
    ASSERT_TRUE(res1.success);
    ASSERT_EQ(res1.object_sha, c1);
    ASSERT_EQ(res1.object_type, "commit");
    ASSERT_TRUE(res1.output.find("commit " + c1) != std::string::npos);
    ASSERT_TRUE(res1.output.find("Author: MiniGit Tester") != std::string::npos);
    ASSERT_TRUE(res1.output.find("Initial commit") != std::string::npos);
    ASSERT_TRUE(res1.output.find("diff --minigit a//dev/null b/hello.txt") != std::string::npos);
    ASSERT_TRUE(res1.output.find("+line 1") != std::string::npos);

    // Commit 2: modify hello.txt and add greeting.txt
    const std::string c2 = commit_files(repo, {
        {"hello.txt", "line 1\nline 2 modified\n"},
        {"greeting.txt", "welcome\n"}
    }, "Second commit");

    // Inspect second commit
    ShowResult res2 = perform_show(repo, c2, ShowFormat::Default);
    ASSERT_TRUE(res2.success);
    ASSERT_EQ(res2.object_sha, c2);
    ASSERT_TRUE(res2.output.find("Second commit") != std::string::npos);
    ASSERT_TRUE(res2.output.find("diff --minigit a/hello.txt b/hello.txt") != std::string::npos);
    ASSERT_TRUE(res2.output.find("-line 2") != std::string::npos);
    ASSERT_TRUE(res2.output.find("+line 2 modified") != std::string::npos);
    ASSERT_TRUE(res2.output.find("diff --minigit a//dev/null b/greeting.txt") != std::string::npos);
    ASSERT_TRUE(res2.output.find("+welcome") != std::string::npos);
}

TEST_CASE(Show, NameOnlyAndStat)
{
    const fs::path repo = create_test_repo("name_only_stat");

    commit_files(repo, {{"f1.txt", "a\nb\n"}}, "First");
    const std::string c2 = commit_files(repo, {
        {"f1.txt", "a\nb\nc\n"},
        {"f2.txt", "new file\n"}
    }, "Second");

    // Test --name-only
    ShowResult res_name = perform_show(repo, "HEAD", ShowFormat::NameOnly);
    ASSERT_TRUE(res_name.success);
    ASSERT_TRUE(res_name.output.find("f1.txt") != std::string::npos);
    ASSERT_TRUE(res_name.output.find("f2.txt") != std::string::npos);
    // Should not contain unified diff headers in --name-only
    ASSERT_TRUE(res_name.output.find("diff --minigit") == std::string::npos);

    // Test --stat
    ShowResult res_stat = perform_show(repo, "HEAD", ShowFormat::Stat);
    ASSERT_TRUE(res_stat.success);
    ASSERT_TRUE(res_stat.output.find("f1.txt |") != std::string::npos);
    ASSERT_TRUE(res_stat.output.find("f2.txt |") != std::string::npos);
    ASSERT_TRUE(res_stat.output.find("2 files changed") != std::string::npos);
}

TEST_CASE(Show, AncestryNavigation)
{
    const fs::path repo = create_test_repo("ancestry");

    const std::string c1 = commit_files(repo, {{"file.txt", "first\n"}}, "First commit");
    const std::string c2 = commit_files(repo, {{"file.txt", "second\n"}}, "Second commit");
    const std::string c3 = commit_files(repo, {{"file.txt", "third\n"}}, "Third commit");

    // HEAD~1 should be c2
    ShowResult res_tilde1 = perform_show(repo, "HEAD~1");
    ASSERT_TRUE(res_tilde1.success);
    ASSERT_EQ(res_tilde1.object_sha, c2);

    // HEAD~2 should be c1
    ShowResult res_tilde2 = perform_show(repo, "HEAD~2");
    ASSERT_TRUE(res_tilde2.success);
    ASSERT_EQ(res_tilde2.object_sha, c1);

    // HEAD^ should be c2
    ShowResult res_caret = perform_show(repo, "HEAD^");
    ASSERT_TRUE(res_caret.success);
    ASSERT_EQ(res_caret.object_sha, c2);
}

TEST_CASE(Show, TreeAndBlob)
{
    const fs::path repo = create_test_repo("tree_blob");

    commit_files(repo, {{"data.txt", "sample content 123"}}, "Commit with blob and tree");

    const fs::path git_dir = repo / ".minigit";
    ObjectDatabase db(git_dir / "objects");
    const std::string head_sha = read_single_line(git_dir / "refs" / "heads" / "main");

    const ParsedCommit c = parse_commit(db.read(head_sha));

    // Inspect Tree object
    ShowResult res_tree = perform_show(repo, c.tree_id);
    ASSERT_TRUE(res_tree.success);
    ASSERT_EQ(res_tree.object_type, "tree");
    ASSERT_TRUE(res_tree.output.find("100644 blob") != std::string::npos);
    ASSERT_TRUE(res_tree.output.find("data.txt") != std::string::npos);

    // Parse tree to get blob id
    const ParsedTree t = parse_tree(db.read(c.tree_id));
    ASSERT_EQ(t.entries.size(), 1);
    const std::string blob_id = t.entries[0].id;

    // Inspect Blob object
    ShowResult res_blob = perform_show(repo, blob_id);
    ASSERT_TRUE(res_blob.success);
    ASSERT_EQ(res_blob.object_type, "blob");
    ASSERT_EQ(res_blob.output, "sample content 123");
}

TEST_CASE(Show, TagInspection)
{
    const fs::path repo = create_test_repo("tags");
    const fs::path git_dir = repo / ".minigit";

    const std::string c1 = commit_files(repo, {{"version.txt", "v1.0\n"}}, "Release v1.0");

    // 1. Lightweight tag
    fs::create_directories(git_dir / "refs" / "tags");
    {
        std::ofstream lt(git_dir / "refs" / "tags" / "v1.0-light");
        lt << c1 << '\n';
    }

    ShowResult res_light = perform_show(repo, "v1.0-light");
    ASSERT_TRUE(res_light.success);
    ASSERT_EQ(res_light.object_sha, c1);
    ASSERT_TRUE(res_light.output.find("Release v1.0") != std::string::npos);

    // 2. Annotated tag
    ObjectDatabase db(git_dir / "objects");
    std::string tag_body;
    tag_body += "object " + c1 + "\n";
    tag_body += "type commit\n";
    tag_body += "tag v1.0-annotated\n";
    tag_body += "tagger Release Lead <lead@minigit> 1700000000\n\n";
    tag_body += "Official release v1.0\n";

    const std::string tag_full = "tag " + std::to_string(tag_body.size()) + '\0' + tag_body;
    Blob b(tag_body); // get a sha or use sha256
    // Use ObjectDatabase write
    const std::string tag_sha = "aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899";
    db.write(tag_sha, tag_full);

    {
        std::ofstream at(git_dir / "refs" / "tags" / "v1.0-annotated");
        at << tag_sha << '\n';
    }

    ShowResult res_annotated = perform_show(repo, "v1.0-annotated");
    ASSERT_TRUE(res_annotated.success);
    ASSERT_EQ(res_annotated.object_type, "tag");
    ASSERT_TRUE(res_annotated.output.find("tag v1.0-annotated") != std::string::npos);
    ASSERT_TRUE(res_annotated.output.find("Tagger: Release Lead") != std::string::npos);
    ASSERT_TRUE(res_annotated.output.find("Official release v1.0") != std::string::npos);
    ASSERT_TRUE(res_annotated.output.find("Release v1.0") != std::string::npos);
}

TEST_CASE(Show, ShortShaAndErrorHandling)
{
    const fs::path repo = create_test_repo("errors_and_prefixes");

    const std::string c1 = commit_files(repo, {{"file.txt", "data\n"}}, "Test short sha");

    // Short SHA lookup (prefix of length 8)
    const std::string prefix = c1.substr(0, 8);
    ShowResult res_prefix = perform_show(repo, prefix);
    ASSERT_TRUE(res_prefix.success);
    ASSERT_EQ(res_prefix.object_sha, c1);

    // Invalid non-existent object
    ShowResult res_bad = perform_show(repo, "non_existent_ref");
    ASSERT_FALSE(res_bad.success);
    ASSERT_TRUE(res_bad.error_message.find("Not a valid object name") != std::string::npos);
}
