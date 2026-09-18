#include "bisect.h"

#include "diff/diff_engine.h"
#include "repository/repository.h"
#include "storage/blob.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"
#include "staging/index.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace minigit::bisect
{
namespace
{

std::string trim(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n'))
        start++;
    return s.substr(start);
}

std::string read_file_text(const fs::path &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return trim(ss.str());
}

void write_file_text(const fs::path &path, const std::string &content)
{
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
        throw std::runtime_error("Could not write file: " + path.string());
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

void append_file_text(const fs::path &path, const std::string &content)
{
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::app);
    if (!f)
        throw std::runtime_error("Could not write file: " + path.string());
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

bool is_all_hex(const std::string &s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

// Bisect state paths
fs::path bisect_start_path(const fs::path &git_dir) { return git_dir / "BISECT_START"; }
fs::path bisect_terms_path(const fs::path &git_dir) { return git_dir / "BISECT_TERMS"; }
fs::path bisect_log_path(const fs::path &git_dir) { return git_dir / "BISECT_LOG"; }
fs::path bisect_expected_rev_path(const fs::path &git_dir) { return git_dir / "BISECT_EXPECTED_REV"; }
fs::path bisect_no_checkout_path(const fs::path &git_dir) { return git_dir / "BISECT_NO_CHECKOUT"; }
fs::path bisect_refs_dir(const fs::path &git_dir) { return git_dir / "refs" / "bisect"; }
fs::path bisect_bad_path(const fs::path &git_dir) { return git_dir / "refs" / "bisect" / "bad"; }

bool is_bisecting(const fs::path &git_dir)
{
    return fs::exists(bisect_start_path(git_dir));
}

struct BisectTerms
{
    std::string term_bad{"bad"};
    std::string term_good{"good"};
};

BisectTerms get_terms(const fs::path &git_dir)
{
    BisectTerms terms;
    const fs::path p = bisect_terms_path(git_dir);
    if (fs::exists(p))
    {
        std::ifstream f(p);
        std::string line;
        if (std::getline(f, line)) terms.term_bad = trim(line);
        if (std::getline(f, line)) terms.term_good = trim(line);
    }
    return terms;
}

void save_terms(const fs::path &git_dir, const BisectTerms &terms)
{
    write_file_text(bisect_terms_path(git_dir), terms.term_bad + "\n" + terms.term_good + "\n");
}

void log_bisect_command(const fs::path &git_dir, const std::string &line)
{
    append_file_text(bisect_log_path(git_dir), line + "\n");
}

std::string get_bad_commit(const fs::path &git_dir)
{
    const fs::path p = bisect_bad_path(git_dir);
    if (fs::exists(p))
        return read_file_text(p);
    const fs::path alt = git_dir / "BISECT_BAD";
    if (fs::exists(alt))
        return read_file_text(alt);
    return {};
}

void set_bad_commit(const fs::path &git_dir, const std::string &sha)
{
    write_file_text(bisect_bad_path(git_dir), sha + "\n");
    write_file_text(git_dir / "BISECT_BAD", sha + "\n");
}

std::vector<std::string> get_good_commits(const fs::path &git_dir)
{
    std::vector<std::string> goods;
    const fs::path dir = bisect_refs_dir(git_dir);
    if (fs::exists(dir))
    {
        for (const auto &entry : fs::directory_iterator(dir))
        {
            if (entry.is_regular_file())
            {
                const std::string name = entry.path().filename().string();
                if (name.rfind("good-", 0) == 0)
                {
                    std::string sha = read_file_text(entry.path());
                    if (!sha.empty())
                        goods.push_back(sha);
                }
            }
        }
    }
    return goods;
}

void add_good_commit(const fs::path &git_dir, const std::string &sha)
{
    write_file_text(bisect_refs_dir(git_dir) / ("good-" + sha), sha + "\n");
}

std::unordered_set<std::string> get_skipped_commits(const fs::path &git_dir)
{
    std::unordered_set<std::string> skips;
    const fs::path dir = bisect_refs_dir(git_dir);
    if (fs::exists(dir))
    {
        for (const auto &entry : fs::directory_iterator(dir))
        {
            if (entry.is_regular_file())
            {
                const std::string name = entry.path().filename().string();
                if (name.rfind("skip-", 0) == 0)
                {
                    std::string sha = read_file_text(entry.path());
                    if (!sha.empty())
                        skips.insert(sha);
                }
            }
        }
    }
    return skips;
}

void add_skip_commit(const fs::path &git_dir, const std::string &sha)
{
    write_file_text(bisect_refs_dir(git_dir) / ("skip-" + sha), sha + "\n");
}

// Resolve revision (HEAD, branch, tag, full SHA, short SHA, ancestry ~N / ^)
std::string resolve_revision(const Repository &repo, ObjectDatabase &db, const std::string &rev_str)
{
    const fs::path git_dir = repo.git_dir();
    std::string rev = trim(rev_str);
    if (rev.empty() || rev == "HEAD")
    {
        return Repository::resolve_head_from_dir(git_dir);
    }

    // Check for ancestry navigation (~N or ^)
    size_t tilde_pos = rev.find('~');
    size_t caret_pos = rev.find('^');
    size_t split_pos = std::string::npos;
    int ancestry_depth = 0;

    if (tilde_pos != std::string::npos)
    {
        split_pos = tilde_pos;
        std::string depth_str = rev.substr(tilde_pos + 1);
        ancestry_depth = depth_str.empty() ? 1 : std::max(1, std::atoi(depth_str.c_str()));
    }
    else if (caret_pos != std::string::npos)
    {
        split_pos = caret_pos;
        std::string depth_str = rev.substr(caret_pos + 1);
        ancestry_depth = depth_str.empty() ? 1 : std::max(1, std::atoi(depth_str.c_str()));
    }

    std::string base_name = (split_pos != std::string::npos) ? rev.substr(0, split_pos) : rev;
    std::string base_sha;

    if (base_name.empty() || base_name == "HEAD")
    {
        base_sha = Repository::resolve_head_from_dir(git_dir);
    }
    else
    {
        // 1. Branch
        fs::path branch_path = Repository::resolve_path(git_dir, "refs/heads/" + base_name);
        if (fs::exists(branch_path))
            base_sha = read_file_text(branch_path);

        // 2. Tag
        if (base_sha.empty())
        {
            fs::path tag_path = Repository::resolve_path(git_dir, "refs/tags/" + base_name);
            if (fs::exists(tag_path))
            {
                std::string tag_sha = read_file_text(tag_path);
                try
                {
                    std::string raw = db.read(tag_sha);
                    if (raw.rfind("tag ", 0) == 0)
                    {
                        size_t null_pos = raw.find('\0');
                        if (null_pos != std::string::npos)
                        {
                            std::istringstream iss(raw.substr(null_pos + 1));
                            std::string line;
                            while (std::getline(iss, line))
                            {
                                if (line.rfind("object ", 0) == 0)
                                {
                                    base_sha = trim(line.substr(7));
                                    break;
                                }
                            }
                        }
                    }
                    if (base_sha.empty()) base_sha = tag_sha;
                }
                catch (...)
                {
                    base_sha = tag_sha;
                }
            }
        }

        // 3. Remote
        if (base_sha.empty())
        {
            fs::path remote_path = Repository::resolve_path(git_dir, "refs/remotes/" + base_name);
            if (fs::exists(remote_path))
                base_sha = read_file_text(remote_path);
        }

        // 4. Exact 64-character SHA
        if (base_sha.empty() && base_name.size() == 64 && is_all_hex(base_name))
        {
            try
            {
                db.read(base_name);
                base_sha = base_name;
            }
            catch (...) {}
        }

        // 5. Short SHA prefix (>= 4 hex chars)
        if (base_sha.empty() && base_name.size() >= 4 && is_all_hex(base_name))
        {
            fs::path prefix_dir = repo.objects_dir() / base_name.substr(0, 2);
            if (fs::exists(prefix_dir))
            {
                std::string rest = base_name.substr(2);
                for (const auto &entry : fs::directory_iterator(prefix_dir))
                {
                    if (entry.is_regular_file())
                    {
                        std::string fname = entry.path().filename().string();
                        if (fname.rfind(rest, 0) == 0)
                        {
                            base_sha = base_name.substr(0, 2) + fname;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (base_sha.empty())
        return {};

    if (ancestry_depth <= 0)
        return base_sha;

    // Follow parent pointers
    std::string curr = base_sha;
    for (int d = 0; d < ancestry_depth; ++d)
    {
        try
        {
            ParsedCommit c = parse_commit(db.read(curr));
            if (c.parent_ids.empty())
                return {};
            curr = c.parent_ids[0];
        }
        catch (...)
        {
            return {};
        }
    }

    return curr;
}

// Clean and checkout commit
void checkout_commit_clean(const Repository &repo, ObjectDatabase &db, const std::string &target_sha)
{
    ParsedCommit commit = parse_commit(db.read(target_sha));
    ParsedTree tree = parse_tree(db.read(commit.tree_id));

    // 1. Remove tracked files in index that are no longer in target tree
    Index old_index(repo.index_path());
    std::unordered_set<std::string> new_names;
    for (const auto &entry : tree.entries)
        new_names.insert(entry.name);

    for (const auto &[path, _] : old_index.entries())
    {
        if (!new_names.count(path))
        {
            std::error_code ec;
            fs::remove(repo.root() / path, ec);
        }
    }

    // 2. Clear index
    {
        std::ofstream clear(repo.index_path(), std::ios::trunc);
    }
    Index fresh_index(repo.index_path());

    // 3. Write files from tree to working directory
    for (const auto &entry : tree.entries)
    {
        if (entry.mode == "160000")
        {
            // Submodule gitlink
            fresh_index.add(entry.name, entry.id);
            continue;
        }

        std::string content = strip_object_header(db.read(entry.id));
        fs::path abs = repo.root() / entry.name;
        fs::create_directories(abs.parent_path());
        std::ofstream f(abs, std::ios::binary | std::ios::trunc);
        if (!f)
            throw std::runtime_error("Cannot write file: " + abs.string());
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
        fresh_index.add(entry.name, entry.id);
    }
    fresh_index.write();

    // 4. Update detached HEAD
    write_file_text(repo.head_path(), target_sha + "\n");
}

void print_commit_summary(const std::string &sha, const ParsedCommit &commit)
{
    std::string first_line;
    size_t nl = commit.message.find('\n');
    if (nl != std::string::npos)
        first_line = commit.message.substr(0, nl);
    else
        first_line = commit.message;

    std::string short_sha = sha.substr(0, 7);
    std::cout << "[" << short_sha << "] " << first_line << "\n";
}

void print_first_bad_commit(ObjectDatabase &db, const std::string &sha)
{
    try
    {
        ParsedCommit c = parse_commit(db.read(sha));
        std::cout << sha << " is the first bad commit\n";
        std::cout << "commit " << sha << "\n";
        std::cout << "Author: " << c.author << "\n";
        std::cout << "Date:   " << c.timestamp << "\n\n";

        // Indent message by 4 spaces
        std::istringstream msg_iss(c.message);
        std::string line;
        while (std::getline(msg_iss, line))
        {
            std::cout << "    " << line << "\n";
        }

        // Print changed files compared to parent if any
        if (!c.parent_ids.empty())
        {
            try
            {
                ParsedCommit pcommit = parse_commit(db.read(c.parent_ids[0]));
                ParsedTree ptree = parse_tree(db.read(pcommit.tree_id));
                ParsedTree ctree = parse_tree(db.read(c.tree_id));

                std::unordered_map<std::string, std::string> pmap;
                for (const auto &e : ptree.entries) pmap[e.name] = e.id;
                std::unordered_map<std::string, std::string> cmap;
                for (const auto &e : ctree.entries) cmap[e.name] = e.id;

                std::cout << "\n";
                for (const auto &[name, id] : cmap)
                {
                    if (!pmap.count(name))
                        std::cout << " create mode 100644 " << name << "\n";
                    else if (pmap[name] != id)
                        std::cout << " modified: " << name << "\n";
                }
                for (const auto &[name, _] : pmap)
                {
                    if (!cmap.count(name))
                        std::cout << " delete mode 100644 " << name << "\n";
                }
            }
            catch (...) {}
        }
    }
    catch (const std::exception &e)
    {
        std::cout << sha << " is the first bad commit\n";
    }
}

// The core DAG bisection step
int perform_bisect_step(const Repository &repo, ObjectDatabase &db, bool interactive = true)
{
    const fs::path git_dir = repo.git_dir();
    std::string bad_sha = get_bad_commit(git_dir);
    std::vector<std::string> good_shas = get_good_commits(git_dir);
    std::unordered_set<std::string> skip_shas = get_skipped_commits(git_dir);

    BisectTerms terms = get_terms(git_dir);

    if (bad_sha.empty() && good_shas.empty())
    {
        if (interactive)
            std::cout << "status: waiting for both " << terms.term_good << " and " << terms.term_bad << " commits\n";
        return 0;
    }
    if (bad_sha.empty())
    {
        if (interactive)
            std::cout << "status: waiting for " << terms.term_bad << " commit, " << terms.term_good << " commit(s) known\n";
        return 0;
    }
    if (good_shas.empty())
    {
        if (interactive)
            std::cout << "status: waiting for " << terms.term_good << " commit(s), " << terms.term_bad << " commit known\n";
        return 0;
    }

    // 1. BFS all commits reachable from bad_sha
    std::unordered_set<std::string> reachable_from_bad;
    {
        std::queue<std::string> q;
        q.push(bad_sha);
        while (!q.empty())
        {
            std::string cur = q.front();
            q.pop();
            if (cur.empty() || reachable_from_bad.count(cur))
                continue;
            reachable_from_bad.insert(cur);
            try
            {
                ParsedCommit c = parse_commit(db.read(cur));
                for (const auto &p : c.parent_ids)
                {
                    if (!p.empty()) q.push(p);
                }
            }
            catch (...) {}
        }
    }

    // 2. BFS all commits reachable from good_shas
    std::unordered_set<std::string> reachable_from_goods;
    {
        std::queue<std::string> q;
        for (const auto &g : good_shas)
            q.push(g);
        while (!q.empty())
        {
            std::string cur = q.front();
            q.pop();
            if (cur.empty() || reachable_from_goods.count(cur))
                continue;
            reachable_from_goods.insert(cur);
            try
            {
                ParsedCommit c = parse_commit(db.read(cur));
                for (const auto &p : c.parent_ids)
                {
                    if (!p.empty()) q.push(p);
                }
            }
            catch (...) {}
        }
    }

    // 3. Sanity check: Inversion
    if (reachable_from_goods.count(bad_sha))
    {
        std::cerr << "fatal: cannot bisect: " << terms.term_good << " commit(s) include the " << terms.term_bad << " commit\n";
        return 1;
    }

    // 4. Candidate set C = reachable(bad) \ reachable(goods)
    std::vector<std::string> candidates;
    std::unordered_set<std::string> candidate_set;
    for (const auto &c : reachable_from_bad)
    {
        if (!reachable_from_goods.count(c))
        {
            candidates.push_back(c);
            candidate_set.insert(c);
        }
    }

    if (candidates.empty())
    {
        std::cout << "No testable commit found.\n";
        return 0;
    }

    // 5. If only bad_sha is in candidates, it is the first bad commit!
    if (candidates.size() == 1 && candidates[0] == bad_sha)
    {
        print_first_bad_commit(db, bad_sha);
        return 0;
    }

    // 6. Select the optimal midpoint commit
    // For each candidate, calculate weight w(c) = number of candidates in candidate_set reachable from c
    const double target_half = candidates.size() / 2.0;
    std::string best_commit;
    double best_dist = 1e9;

    for (const auto &c : candidates)
    {
        int weight = 0;
        std::unordered_set<std::string> seen;
        std::queue<std::string> wq;
        wq.push(c);
        while (!wq.empty())
        {
            std::string cur = wq.front();
            wq.pop();
            if (cur.empty() || seen.count(cur)) continue;
            seen.insert(cur);
            weight++;
            try
            {
                ParsedCommit pc = parse_commit(db.read(cur));
                for (const auto &p : pc.parent_ids)
                {
                    if (candidate_set.count(p))
                        wq.push(p);
                }
            }
            catch (...) {}
        }

        double dist = std::abs(weight - target_half);
        if (skip_shas.count(c))
            dist += 1000000.0; // penalize skipped commits

        if (dist < best_dist)
        {
            best_dist = dist;
            best_commit = c;
        }
    }

    if (best_commit.empty())
        best_commit = candidates[0];

    // Compute remaining revisions & steps estimate
    int revisions_left = static_cast<int>(candidates.size()) - 1;
    int steps = 0;
    int tmp = static_cast<int>(candidates.size());
    while (tmp > 1)
    {
        tmp /= 2;
        steps++;
    }

    std::cout << "Bisecting: " << revisions_left << " revisions left to test after this (roughly " << steps << " steps)\n";

    ParsedCommit midpoint_commit = parse_commit(db.read(best_commit));
    print_commit_summary(best_commit, midpoint_commit);

    // Unless --no-checkout was specified, checkout midpoint commit
    bool no_checkout = fs::exists(bisect_no_checkout_path(git_dir));
    if (!no_checkout)
    {
        checkout_commit_clean(repo, db, best_commit);
    }
    write_file_text(bisect_expected_rev_path(git_dir), best_commit + "\n");

    return 0;
}

// Subcommands:

int bisect_start(const Repository &repo, ObjectDatabase &db, int argc, const char *argv[])
{
    const fs::path git_dir = repo.git_dir();

    // Read current head to save to BISECT_START
    std::string orig_head = Repository::resolve_head_from_dir(git_dir);
    if (orig_head.empty())
    {
        std::cerr << "fatal: your current branch has no commits yet\n";
        return 1;
    }

    // Determine current branch name or SHA
    std::string head_content = trim(read_file_text(repo.head_path()));
    std::string start_target = head_content;
    if (start_target.rfind("ref: refs/heads/", 0) == 0)
        start_target = start_target.substr(16);
    else if (start_target.rfind("ref: ", 0) == 0)
        start_target = start_target.substr(5);

    // Reset any prior bisect state
    fs::remove_all(bisect_refs_dir(git_dir));
    fs::remove(bisect_start_path(git_dir));
    fs::remove(bisect_terms_path(git_dir));
    fs::remove(bisect_log_path(git_dir));
    fs::remove(bisect_expected_rev_path(git_dir));
    fs::remove(bisect_no_checkout_path(git_dir));
    fs::remove(git_dir / "BISECT_BAD");

    // Initialize state
    write_file_text(bisect_start_path(git_dir), start_target + "\n");
    BisectTerms terms{"bad", "good"};
    save_terms(git_dir, terms);
    log_bisect_command(git_dir, "git bisect start");

    bool no_checkout = false;
    std::vector<std::string> positional_revs;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--no-checkout")
        {
            no_checkout = true;
            write_file_text(bisect_no_checkout_path(git_dir), "1\n");
        }
        else if (arg == "--term-new" || arg == "--term-bad")
        {
            if (i + 1 < argc)
            {
                terms.term_bad = argv[++i];
                save_terms(git_dir, terms);
            }
        }
        else if (arg == "--term-old" || arg == "--term-good")
        {
            if (i + 1 < argc)
            {
                terms.term_good = argv[++i];
                save_terms(git_dir, terms);
            }
        }
        else if (arg == "--")
        {
            // Pathspec separator
            break;
        }
        else if (!arg.starts_with("-"))
        {
            positional_revs.push_back(arg);
        }
    }

    if (!positional_revs.empty())
    {
        // First positional is bad
        std::string bad_sha = resolve_revision(repo, db, positional_revs[0]);
        if (bad_sha.empty())
        {
            std::cerr << "fatal: invalid revision: '" << positional_revs[0] << "'\n";
            return 1;
        }
        set_bad_commit(git_dir, bad_sha);
        log_bisect_command(git_dir, "git bisect bad " + bad_sha);

        // Subsequent positional are good
        for (size_t i = 1; i < positional_revs.size(); ++i)
        {
            std::string good_sha = resolve_revision(repo, db, positional_revs[i]);
            if (good_sha.empty())
            {
                std::cerr << "fatal: invalid revision: '" << positional_revs[i] << "'\n";
                return 1;
            }
            add_good_commit(git_dir, good_sha);
            log_bisect_command(git_dir, "git bisect good " + good_sha);
        }
    }

    return perform_bisect_step(repo, db, true);
}

int bisect_bad(const Repository &repo, ObjectDatabase &db, int argc, const char *argv[])
{
    const fs::path git_dir = repo.git_dir();
    if (!is_bisecting(git_dir))
    {
        // Auto-start if not already started
        const char *start_argv[] = {"minigit", "bisect", "start"};
        bisect_start(repo, db, 3, start_argv);
    }

    BisectTerms terms = get_terms(git_dir);
    std::string rev = (argc >= 4) ? argv[3] : "HEAD";
    std::string sha = resolve_revision(repo, db, rev);
    if (sha.empty())
    {
        std::cerr << "fatal: Bad rev input: " << rev << "\n";
        return 1;
    }

    set_bad_commit(git_dir, sha);
    log_bisect_command(git_dir, "git bisect " + terms.term_bad + " " + sha);

    return perform_bisect_step(repo, db, true);
}

int bisect_good(const Repository &repo, ObjectDatabase &db, int argc, const char *argv[])
{
    const fs::path git_dir = repo.git_dir();
    if (!is_bisecting(git_dir))
    {
        // Auto-start if not already started
        const char *start_argv[] = {"minigit", "bisect", "start"};
        bisect_start(repo, db, 3, start_argv);
    }

    BisectTerms terms = get_terms(git_dir);
    std::vector<std::string> revs;
    if (argc >= 4)
    {
        for (int i = 3; i < argc; ++i)
            revs.push_back(argv[i]);
    }
    else
    {
        revs.push_back("HEAD");
    }

    for (const auto &r : revs)
    {
        std::string sha = resolve_revision(repo, db, r);
        if (sha.empty())
        {
            std::cerr << "fatal: Bad rev input: " << r << "\n";
            return 1;
        }
        add_good_commit(git_dir, sha);
        log_bisect_command(git_dir, "git bisect " + terms.term_good + " " + sha);
    }

    return perform_bisect_step(repo, db, true);
}

int bisect_skip(const Repository &repo, ObjectDatabase &db, int argc, const char *argv[])
{
    const fs::path git_dir = repo.git_dir();
    if (!is_bisecting(git_dir))
    {
        std::cerr << "fatal: We are not bisecting.\n";
        return 1;
    }

    std::vector<std::string> revs;
    if (argc >= 4)
    {
        for (int i = 3; i < argc; ++i)
            revs.push_back(argv[i]);
    }
    else
    {
        revs.push_back("HEAD");
    }

    for (const auto &r : revs)
    {
        std::string sha = resolve_revision(repo, db, r);
        if (sha.empty())
        {
            std::cerr << "fatal: Bad rev input: " << r << "\n";
            return 1;
        }
        add_skip_commit(git_dir, sha);
        log_bisect_command(git_dir, "git bisect skip " + sha);
    }

    return perform_bisect_step(repo, db, true);
}

int bisect_reset(const Repository &repo, ObjectDatabase &db, int argc, const char *argv[])
{
    const fs::path git_dir = repo.git_dir();
    std::string target;

    if (argc >= 4)
    {
        target = argv[3];
    }
    else if (is_bisecting(git_dir))
    {
        target = read_file_text(bisect_start_path(git_dir));
    }

    if (target.empty())
    {
        if (!is_bisecting(git_dir))
        {
            std::cerr << "fatal: We are not bisecting.\n";
            return 1;
        }
        target = "main";
    }

    // Resolve target to branch or commit
    bool is_branch = false;
    fs::path branch_path = Repository::resolve_path(git_dir, "refs/heads/" + target);
    if (fs::exists(branch_path))
    {
        is_branch = true;
    }

    std::string commit_sha = resolve_revision(repo, db, target);
    if (commit_sha.empty())
    {
        std::cerr << "fatal: Could not resolve target: " << target << "\n";
        return 1;
    }

    // Restore working tree and index
    checkout_commit_clean(repo, db, commit_sha);

    // Update HEAD
    if (is_branch)
    {
        write_file_text(repo.head_path(), "ref: refs/heads/" + target + "\n");
        std::cout << "Switched to branch '" << target << "'\n";
    }
    else
    {
        write_file_text(repo.head_path(), commit_sha + "\n");
        std::cout << "HEAD is now at " << commit_sha.substr(0, 7) << "\n";
    }

    // Clean up all bisect state
    fs::remove_all(bisect_refs_dir(git_dir));
    fs::remove(bisect_start_path(git_dir));
    fs::remove(bisect_terms_path(git_dir));
    fs::remove(bisect_log_path(git_dir));
    fs::remove(bisect_expected_rev_path(git_dir));
    fs::remove(bisect_no_checkout_path(git_dir));
    fs::remove(git_dir / "BISECT_BAD");

    return 0;
}

int bisect_terms(const Repository &repo, int argc, const char *argv[])
{
    const fs::path git_dir = repo.git_dir();
    BisectTerms terms = get_terms(git_dir);

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--term-bad" || arg == "--term-new")
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                terms.term_bad = argv[++i];
                save_terms(git_dir, terms);
            }
            else
            {
                std::cout << terms.term_bad << "\n";
                return 0;
            }
        }
        else if (arg == "--term-good" || arg == "--term-old")
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                terms.term_good = argv[++i];
                save_terms(git_dir, terms);
            }
            else
            {
                std::cout << terms.term_good << "\n";
                return 0;
            }
        }
    }

    std::cout << "Your current terms are " << terms.term_bad
              << " for the condition and " << terms.term_good
              << " for the condition.\n";
    return 0;
}

int bisect_log(const Repository &repo)
{
    const fs::path p = bisect_log_path(repo.git_dir());
    if (fs::exists(p))
    {
        std::ifstream f(p, std::ios::binary);
        std::cout << f.rdbuf();
    }
    return 0;
}

int bisect_run(const Repository &repo, ObjectDatabase &db, int argc, const char *argv[])
{
    const fs::path git_dir = repo.git_dir();
    if (!is_bisecting(git_dir))
    {
        std::cerr << "fatal: bisect run failed: no bisect session active\n";
        return 1;
    }

    if (argc < 4)
    {
        std::cerr << "usage: minigit bisect run <cmd>...\n";
        return 1;
    }

    std::string bad_sha = get_bad_commit(git_dir);
    auto goods = get_good_commits(git_dir);
    if (bad_sha.empty() || goods.empty())
    {
        std::cerr << "fatal: bisect run requires both good and bad commits\n";
        return 1;
    }

    std::string full_cmd;
    for (int i = 3; i < argc; ++i)
    {
        if (i > 3) full_cmd += ' ';
        full_cmd += argv[i];
    }

    while (true)
    {
        std::string cur_bad = get_bad_commit(git_dir);
        auto cur_goods = get_good_commits(git_dir);

        // Check if finished (candidates.size() <= 1)
        // We can check this by testing if candidate set size <= 1
        std::unordered_set<std::string> r_bad;
        {
            std::queue<std::string> q;
            q.push(cur_bad);
            while (!q.empty())
            {
                std::string c = q.front(); q.pop();
                if (r_bad.count(c)) continue;
                r_bad.insert(c);
                try {
                    ParsedCommit pc = parse_commit(db.read(c));
                    for (const auto &p : pc.parent_ids) q.push(p);
                } catch (...) {}
            }
        }
        std::unordered_set<std::string> r_goods;
        {
            std::queue<std::string> q;
            for (const auto &g : cur_goods) q.push(g);
            while (!q.empty())
            {
                std::string c = q.front(); q.pop();
                if (r_goods.count(c)) continue;
                r_goods.insert(c);
                try {
                    ParsedCommit pc = parse_commit(db.read(c));
                    for (const auto &p : pc.parent_ids) q.push(p);
                } catch (...) {}
            }
        }

        std::vector<std::string> cand;
        for (const auto &c : r_bad)
        {
            if (!r_goods.count(c)) cand.push_back(c);
        }

        if (cand.size() <= 1)
        {
            std::cout << "bisect run success\n";
            return 0;
        }

        std::cout << "running " << full_cmd << "\n";
        int ret = std::system(full_cmd.c_str());
        int exit_code = ret;
#if !defined(_WIN32)
        if (WIFEXITED(ret)) exit_code = WEXITSTATUS(ret);
#endif

        if (exit_code == 0)
        {
            const char *good_argv[] = {"minigit", "bisect", "good"};
            int res = bisect_good(repo, db, 3, good_argv);
            if (res != 0) return res;
        }
        else if (exit_code == 125)
        {
            const char *skip_argv[] = {"minigit", "bisect", "skip"};
            int res = bisect_skip(repo, db, 3, skip_argv);
            if (res != 0) return res;
        }
        else if (exit_code >= 1 && exit_code <= 127)
        {
            const char *bad_argv[] = {"minigit", "bisect", "bad"};
            int res = bisect_bad(repo, db, 3, bad_argv);
            if (res != 0) return res;
        }
        else
        {
            std::cerr << "bisect run failed: exit code " << exit_code << " from command '" << full_cmd << "'\n";
            return exit_code;
        }
    }
}

int bisect_replay(const Repository &repo, ObjectDatabase &db, int argc, const char *argv[])
{
    if (argc < 4)
    {
        std::cerr << "usage: minigit bisect replay <logfile>\n";
        return 1;
    }

    fs::path log_path = argv[3];
    if (!fs::exists(log_path))
    {
        std::cerr << "fatal: cannot read file '" << log_path.string() << "'\n";
        return 1;
    }

    std::ifstream f(log_path);
    std::string line;
    while (std::getline(f, line))
    {
        line = trim(line);
        if (line.empty() || line.starts_with("#"))
            continue;

        if (line.starts_with("git bisect "))
            line = line.substr(11);
        else if (line.starts_with("minigit bisect "))
            line = line.substr(15);
        else if (line.starts_with("bisect "))
            line = line.substr(7);

        std::istringstream iss(line);
        std::vector<std::string> tokens;
        tokens.push_back("minigit");
        tokens.push_back("bisect");
        std::string tok;
        while (iss >> tok)
            tokens.push_back(tok);

        std::vector<const char *> c_argv;
        for (const auto &t : tokens)
            c_argv.push_back(t.c_str());

        int res = bisect_command(static_cast<int>(c_argv.size()), c_argv.data());
        if (res != 0)
            return res;
    }

    return 0;
}

void print_help()
{
    std::cout << "usage: minigit bisect [help|start|bad|good|new|old|skip|reset|terms|log|replay|run] [<args>...]\n\n";
    std::cout << "Available subcommands:\n";
    std::cout << "   start [<bad> [<good>...]] [--no-checkout]   Start bisection session\n";
    std::cout << "   bad [<rev>]                                Mark revision as bad / regression\n";
    std::cout << "   good [<rev>...]                            Mark revision(s) as good / clean\n";
    std::cout << "   new [<rev>]                                Mark revision as new (alias for bad)\n";
    std::cout << "   old [<rev>...]                             Mark revision(s) as old (alias for good)\n";
    std::cout << "   skip [(<rev>|<range>)...]                  Skip untestable revision\n";
    std::cout << "   reset [<commit>]                           Finish bisection and restore starting branch\n";
    std::cout << "   terms [--term-bad | --term-good]           Inspect or customize bisect terms\n";
    std::cout << "   log                                        Show log of current bisect session\n";
    std::cout << "   replay <logfile>                           Replay commands from bisect log\n";
    std::cout << "   run <cmd> [<arg>...]                       Run automated bisection script\n";
}

} // namespace

int bisect_command(int argc, const char *argv[])
{
    std::string subcmd = (argc >= 3) ? argv[2] : "";

    if (argc < 3 || subcmd == "help" || subcmd == "-h" || subcmd == "--help")
    {
        print_help();
        return 0;
    }

    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    ObjectDatabase db(repo.objects_dir());

    BisectTerms terms = get_terms(repo.git_dir());

    if (subcmd == "start")
        return bisect_start(repo, db, argc, argv);
    if (subcmd == "bad" || subcmd == "new" || subcmd == terms.term_bad)
        return bisect_bad(repo, db, argc, argv);
    if (subcmd == "good" || subcmd == "old" || subcmd == terms.term_good)
        return bisect_good(repo, db, argc, argv);
    if (subcmd == "skip")
        return bisect_skip(repo, db, argc, argv);
    if (subcmd == "reset")
        return bisect_reset(repo, db, argc, argv);
    if (subcmd == "terms")
        return bisect_terms(repo, argc, argv);
    if (subcmd == "log")
        return bisect_log(repo);
    if (subcmd == "replay")
        return bisect_replay(repo, db, argc, argv);
    if (subcmd == "run")
        return bisect_run(repo, db, argc, argv);
    if (subcmd == "visualize" || subcmd == "view")
        return bisect_log(repo);

    std::cerr << "error: unknown subcommand: '" << subcmd << "'\n";
    print_help();
    return 1;
}

} // namespace minigit::bisect
