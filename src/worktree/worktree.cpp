#include "worktree.h"

#include "repository/repository.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"
#include "storage/blob.h"
#include "staging/index.h"
#include "staging/ignore.h"
#include "core/file.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <set>
#include <unordered_map>

namespace minigit::worktree {

namespace {

std::string trim(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n'))
        start++;
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n'))
        end--;
    return s.substr(start, end - start);
}

std::string read_trimmed(const std::filesystem::path& p)
{
    if (!std::filesystem::exists(p))
        return {};
    std::ifstream f(p);
    std::string line;
    if (std::getline(f, line))
        return trim(line);
    return {};
}

void extract_tree_entries(
    const ObjectDatabase& db,
    const std::string& tree_id,
    const std::string& prefix,
    std::vector<std::pair<std::string, std::string>>& out_files)
{
    ParsedTree tree = parse_tree(db.read(tree_id));
    for (const auto& entry : tree.entries)
    {
        std::string full_path = prefix.empty() ? entry.name : (prefix + "/" + entry.name);
        if (entry.mode == "040000")
        {
            extract_tree_entries(db, entry.id, full_path, out_files);
        }
        else
        {
            out_files.emplace_back(full_path, entry.id);
        }
    }
}

std::string resolve_commit_ish(const Repository& repo, const std::string& target)
{
    const auto common_dir = repo.common_dir();
    ObjectDatabase db(repo.objects_dir());

    // 1. Branch: refs/heads/<target>
    const auto branch_ref = common_dir / "refs" / "heads" / target;
    if (std::filesystem::exists(branch_ref))
    {
        return read_trimmed(branch_ref);
    }

    // 2. Tag: refs/tags/<target>
    const auto tag_ref = common_dir / "refs" / "tags" / target;
    if (std::filesystem::exists(tag_ref))
    {
        std::string tag_content = read_trimmed(tag_ref);
        try
        {
            std::string raw = db.read(tag_content);
            if (raw.rfind("tag ", 0) == 0)
            {
                std::string body = strip_object_header(raw);
                std::istringstream iss(body);
                std::string line;
                while (std::getline(iss, line))
                {
                    line = trim(line);
                    if (line.rfind("object ", 0) == 0)
                    {
                        return line.substr(7);
                    }
                }
            }
            return tag_content;
        }
        catch (...)
        {
            return tag_content;
        }
    }

    // 3. Hex SHA
    if (target.size() >= 4 && target.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos)
    {
        if (target.size() == 64 && db.contains(target))
        {
            return target;
        }
        std::string prefix2 = target.substr(0, 2);
        std::string rem = target.substr(2);
        auto pdir = repo.objects_dir() / prefix2;
        if (std::filesystem::exists(pdir))
        {
            std::vector<std::string> matches;
            for (const auto& entry : std::filesystem::directory_iterator(pdir))
            {
                std::string fname = entry.path().filename().string();
                if (fname.rfind(rem, 0) == 0)
                {
                    matches.push_back(prefix2 + fname);
                }
            }
            if (matches.size() == 1)
                return matches[0];
        }
    }

    return {};
}

bool is_worktree_dirty(const WorktreeInfo& wt, const Repository& repo)
{
    if (!std::filesystem::exists(wt.path))
        return false;

    Index index(wt.git_dir / "index");
    ObjectDatabase db(repo.objects_dir());

    IgnoreRules ignore_rules = IgnoreRules::load(wt.path);
    std::set<std::string> disk_files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(wt.path, ec))
    {
        if (!entry.is_regular_file())
            continue;
        auto rel = std::filesystem::relative(entry.path(), wt.path).lexically_normal();
        std::string rel_str = rel.generic_string();
        if (rel_str == ".minigit" || rel_str.starts_with(".minigit/") ||
            rel_str == ".git" || rel_str.starts_with(".git/") ||
            rel_str == ".minigitignore")
            continue;
        if (ignore_rules.is_ignored(rel_str))
            continue;

        disk_files.insert(rel_str);
        if (index.entries().count(rel_str) == 0)
        {
            return true;
        }
        else
        {
            try
            {
                std::string content = read_file(entry.path());
                Blob b(std::move(content));
                if (b.id() != index.entries().at(rel_str))
                    return true;
            }
            catch (...)
            {
                return true;
            }
        }
    }

    for (const auto& [staged_path, _] : index.entries())
    {
        if (disk_files.count(staged_path) == 0)
            return true;
    }

    if (!wt.head_sha.empty())
    {
        try
        {
            ParsedCommit commit = parse_commit(db.read(wt.head_sha));
            std::vector<std::pair<std::string, std::string>> head_files;
            extract_tree_entries(db, commit.tree_id, "", head_files);
            std::unordered_map<std::string, std::string> head_map;
            for (const auto& [p, id] : head_files)
                head_map[p] = id;

            if (index.entries().size() != head_map.size())
                return true;
            for (const auto& [p, id] : index.entries())
            {
                auto it = head_map.find(p);
                if (it == head_map.end() || it->second != id)
                    return true;
            }
        }
        catch (...)
        {
        }
    }

    return false;
}

} // namespace

std::vector<WorktreeInfo> get_all_worktrees(const Repository& repo)
{
    std::vector<WorktreeInfo> list;

    // 1. Main worktree
    const auto main_common_dir = repo.common_dir();
    const auto main_root = std::filesystem::weakly_canonical(main_common_dir.parent_path());
    const auto main_head_file = main_common_dir / "HEAD";

    WorktreeInfo main_wt;
    main_wt.id = "main";
    main_wt.path = main_root;
    main_wt.git_dir = main_common_dir;
    main_wt.is_main = true;

    if (std::filesystem::exists(main_head_file))
    {
        std::string line = read_trimmed(main_head_file);
        if (line.rfind("ref: ", 0) == 0)
        {
            main_wt.branch = line.substr(5);
            main_wt.is_detached = false;
            std::filesystem::path ref_path = main_common_dir / main_wt.branch;
            if (std::filesystem::exists(ref_path))
            {
                main_wt.head_sha = read_trimmed(ref_path);
            }
        }
        else
        {
            main_wt.head_sha = line;
            main_wt.is_detached = true;
        }
    }
    list.push_back(std::move(main_wt));

    // 2. Linked worktrees
    const auto wts_dir = main_common_dir / "worktrees";
    if (std::filesystem::exists(wts_dir))
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(wts_dir, ec))
        {
            if (!entry.is_directory())
                continue;

            WorktreeInfo info;
            info.id = entry.path().filename().string();
            info.git_dir = entry.path();
            info.is_main = false;

            const auto gitdir_file = entry.path() / "gitdir";
            const auto head_file = entry.path() / "HEAD";
            const auto locked_file = entry.path() / "locked";

            if (std::filesystem::exists(locked_file))
            {
                info.is_locked = true;
                info.lock_reason = read_trimmed(locked_file);
            }

            if (!std::filesystem::exists(gitdir_file))
            {
                info.is_prunable = true;
                info.prunable_reason = "gitdir file does not exist";
            }
            else
            {
                std::string target_gitdir = read_trimmed(gitdir_file);
                std::filesystem::path tg_path(target_gitdir);
                std::filesystem::path wt_root = tg_path.parent_path();
                info.path = std::filesystem::weakly_canonical(wt_root);

                if (!std::filesystem::exists(tg_path) || !std::filesystem::exists(wt_root))
                {
                    info.is_prunable = true;
                    info.prunable_reason = "gitdir file points to non-existent location";
                }
            }

            if (std::filesystem::exists(head_file))
            {
                std::string hline = read_trimmed(head_file);
                if (hline.rfind("ref: ", 0) == 0)
                {
                    info.branch = hline.substr(5);
                    info.is_detached = false;
                    std::filesystem::path ref_path = main_common_dir / info.branch;
                    if (std::filesystem::exists(ref_path))
                    {
                        info.head_sha = read_trimmed(ref_path);
                    }
                }
                else
                {
                    info.head_sha = hline;
                    info.is_detached = true;
                }
            }

            list.push_back(std::move(info));
        }
    }

    return list;
}

int worktree_list(int argc, char const* argv[])
{
    bool porcelain = false;
    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--porcelain")
            porcelain = true;
    }

    Repository repo(std::filesystem::current_path());
    try
    {
        repo = Repository::discover(std::filesystem::current_path());
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    auto worktrees = get_all_worktrees(repo);

    if (porcelain)
    {
        for (size_t i = 0; i < worktrees.size(); ++i)
        {
            const auto& wt = worktrees[i];
            std::cout << "worktree " << wt.path.generic_string() << '\n';
            std::cout << "HEAD " << (wt.head_sha.empty() ? "0000000000000000000000000000000000000000" : wt.head_sha) << '\n';
            if (wt.is_detached)
            {
                std::cout << "detached\n";
            }
            else if (!wt.branch.empty())
            {
                std::string b = wt.branch.starts_with("refs/") ? wt.branch : ("refs/heads/" + wt.branch);
                std::cout << "branch " << b << '\n';
            }
            if (wt.is_locked)
            {
                std::cout << "locked" << (wt.lock_reason.empty() ? "" : " " + wt.lock_reason) << '\n';
            }
            if (wt.is_prunable)
            {
                std::cout << "prunable " << wt.prunable_reason << '\n';
            }
            std::cout << '\n';
        }
    }
    else
    {
        for (const auto& wt : worktrees)
        {
            std::string short_sha = wt.head_sha.empty() ? "0000000" : wt.head_sha.substr(0, 7);
            std::string branch_str;
            if (wt.is_detached)
            {
                branch_str = "(detached HEAD)";
            }
            else if (!wt.branch.empty())
            {
                std::string b = wt.branch;
                if (b.rfind("refs/heads/", 0) == 0)
                    b = b.substr(11);
                branch_str = "[" + b + "]";
            }

            std::cout << wt.path.generic_string() << "  " << short_sha << " " << branch_str;
            if (wt.is_locked)
            {
                std::cout << " locked" << (wt.lock_reason.empty() ? "" : ": " + wt.lock_reason);
            }
            if (wt.is_prunable)
            {
                std::cout << " [prunable]";
            }
            std::cout << '\n';
        }
    }

    return 0;
}

int worktree_add(int argc, char const* argv[])
{
    bool detach = false;
    bool force = false;
    bool reset_branch = false;
    std::string new_branch_name;
    std::string path_arg;
    std::string commit_ish;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--detach" || arg == "-d")
        {
            detach = true;
        }
        else if (arg == "-f" || arg == "--force")
        {
            force = true;
        }
        else if (arg == "-b" && i + 1 < argc)
        {
            new_branch_name = argv[++i];
        }
        else if (arg == "-B" && i + 1 < argc)
        {
            new_branch_name = argv[++i];
            reset_branch = true;
        }
        else if (arg.rfind("-b=", 0) == 0)
        {
            new_branch_name = arg.substr(3);
        }
        else if (arg.rfind("-B=", 0) == 0)
        {
            new_branch_name = arg.substr(3);
            reset_branch = true;
        }
        else if (arg.rfind("-", 0) == 0)
        {
            std::cerr << "fatal: unknown switch '" << arg << "'\n";
            return 1;
        }
        else
        {
            if (path_arg.empty())
            {
                path_arg = arg;
            }
            else if (commit_ish.empty())
            {
                commit_ish = arg;
            }
            else
            {
                std::cerr << "usage: minigit worktree add [-b <new-branch>] [--detach] [-f] <path> [<commit-ish>]\n";
                return 1;
            }
        }
    }

    if (path_arg.empty())
    {
        std::cerr << "usage: minigit worktree add [-b <new-branch>] [--detach] [-f] <path> [<commit-ish>]\n";
        return 1;
    }

    if (detach && !new_branch_name.empty())
    {
        std::cerr << "fatal: --detach and -b cannot be used together\n";
        return 1;
    }

    Repository repo(std::filesystem::current_path());
    try
    {
        repo = Repository::discover(std::filesystem::current_path());
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    std::filesystem::path target_path = std::filesystem::weakly_canonical(
        std::filesystem::absolute(path_arg));

    if (std::filesystem::exists(target_path))
    {
        if (!std::filesystem::is_directory(target_path) || !std::filesystem::is_empty(target_path))
        {
            std::cerr << "fatal: '" << path_arg << "' already exists\n";
            return 1;
        }
    }

    std::string branch_name;
    std::string start_sha;
    bool is_new_branch = false;

    if (!new_branch_name.empty())
    {
        branch_name = new_branch_name;
        std::filesystem::path branch_file = repo.common_dir() / "refs" / "heads" / branch_name;
        if (std::filesystem::exists(branch_file) && !reset_branch)
        {
            std::cerr << "fatal: a branch named '" << branch_name << "' already exists\n";
            return 1;
        }

        if (!commit_ish.empty())
        {
            start_sha = resolve_commit_ish(repo, commit_ish);
            if (start_sha.empty())
            {
                std::cerr << "fatal: invalid reference: " << commit_ish << '\n';
                return 1;
            }
        }
        else
        {
            start_sha = Repository::resolve_head_from_dir(repo.git_dir());
        }
        is_new_branch = true;
    }
    else if (detach)
    {
        if (!commit_ish.empty())
        {
            start_sha = resolve_commit_ish(repo, commit_ish);
            if (start_sha.empty())
            {
                std::cerr << "fatal: invalid reference: " << commit_ish << '\n';
                return 1;
            }
        }
        else
        {
            start_sha = Repository::resolve_head_from_dir(repo.git_dir());
        }
    }
    else if (!commit_ish.empty())
    {
        std::filesystem::path branch_file = repo.common_dir() / "refs" / "heads" / commit_ish;
        if (std::filesystem::exists(branch_file))
        {
            branch_name = commit_ish;
            start_sha = read_trimmed(branch_file);
        }
        else
        {
            start_sha = resolve_commit_ish(repo, commit_ish);
            if (start_sha.empty())
            {
                std::cerr << "fatal: invalid reference: " << commit_ish << '\n';
                return 1;
            }
            detach = true;
        }
    }
    else
    {
        branch_name = target_path.filename().string();
        std::filesystem::path branch_file = repo.common_dir() / "refs" / "heads" / branch_name;
        if (std::filesystem::exists(branch_file))
        {
            start_sha = read_trimmed(branch_file);
        }
        else
        {
            start_sha = Repository::resolve_head_from_dir(repo.git_dir());
            is_new_branch = true;
        }
    }

    if (start_sha.empty())
    {
        std::cerr << "fatal: not a valid object name: 'HEAD' (no commits yet)\n";
        return 1;
    }

    if (!detach && !force)
    {
        auto match = repo.find_branch_worktree(branch_name);
        if (match.is_checked_out)
        {
            std::cerr << "fatal: '" << branch_name << "' is already checked out at '"
                      << match.worktree_path.generic_string() << "'\n";
            return 1;
        }
    }

    // Write new branch ref if created
    if (!detach && is_new_branch)
    {
        std::filesystem::path branch_file = repo.common_dir() / "refs" / "heads" / branch_name;
        std::filesystem::create_directories(branch_file.parent_path());
        std::ofstream bf(branch_file, std::ios::trunc);
        if (!bf)
        {
            std::cerr << "fatal: could not create branch ref\n";
            return 1;
        }
        bf << start_sha << '\n';
    }

    ObjectDatabase db(repo.objects_dir());
    ParsedCommit commit;
    try
    {
        commit = parse_commit(db.read(start_sha));
    }
    catch (const std::exception& e)
    {
        std::cerr << "fatal: " << e.what() << '\n';
        return 1;
    }

    // Generate unique worktree ID
    std::string base_id = target_path.filename().string();
    if (base_id.empty() || base_id == "." || base_id == "..")
        base_id = "worktree";

    std::string wt_id = base_id;
    int id_suffix = 1;
    while (std::filesystem::exists(repo.common_dir() / "worktrees" / wt_id))
    {
        wt_id = base_id + std::to_string(id_suffix++);
    }

    std::filesystem::path wt_meta_dir = repo.common_dir() / "worktrees" / wt_id;
    std::filesystem::create_directories(wt_meta_dir);

    // Write gitdir pointer
    {
        std::ofstream gf(wt_meta_dir / "gitdir", std::ios::trunc);
        gf << (target_path / ".minigit").generic_string() << '\n';
    }

    // Write commondir pointer
    {
        std::ofstream cf(wt_meta_dir / "commondir", std::ios::trunc);
        auto rel_common = std::filesystem::relative(repo.common_dir(), wt_meta_dir);
        cf << rel_common.generic_string() << '\n';
    }

    // Write HEAD
    {
        std::ofstream hf(wt_meta_dir / "HEAD", std::ios::trunc);
        if (detach)
        {
            hf << start_sha << '\n';
        }
        else
        {
            hf << "ref: refs/heads/" << branch_name << '\n';
        }
    }

    // Create target directory and .minigit file
    std::filesystem::create_directories(target_path);
    {
        std::ofstream mf(target_path / ".minigit", std::ios::trunc);
        mf << "gitdir: " << wt_meta_dir.generic_string() << '\n';
    }

    // Checkout tree files and write index
    std::vector<std::pair<std::string, std::string>> files;
    extract_tree_entries(db, commit.tree_id, "", files);

    Index fresh_index(wt_meta_dir / "index");
    for (const auto& [rel_p, blob_id] : files)
    {
        try
        {
            std::string content = strip_object_header(db.read(blob_id));
            std::filesystem::path dest = target_path / rel_p;
            std::filesystem::create_directories(dest.parent_path());
            std::ofstream out(dest, std::ios::binary | std::ios::trunc);
            if (out)
            {
                out.write(content.data(), static_cast<std::streamsize>(content.size()));
            }
            fresh_index.add(rel_p, blob_id);
        }
        catch (const std::exception& e)
        {
            std::cerr << "error restoring '" << rel_p << "': " << e.what() << '\n';
        }
    }
    fresh_index.write();

    if (detach)
    {
        std::cout << "Preparing worktree (detached HEAD " << start_sha.substr(0, 7) << ")\n";
    }
    else if (is_new_branch)
    {
        std::cout << "Preparing worktree (new branch '" << branch_name << "')\n";
    }
    else
    {
        std::cout << "Preparing worktree (checking out '" << branch_name << "')\n";
    }
    std::cout << "HEAD is now at " << start_sha.substr(0, 7) << " " << commit.message << '\n';

    return 0;
}

int worktree_remove(int argc, char const* argv[])
{
    bool force = false;
    std::string target;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-f" || arg == "--force")
        {
            force = true;
        }
        else if (arg.rfind("-", 0) == 0)
        {
            std::cerr << "fatal: unknown switch '" << arg << "'\n";
            return 1;
        }
        else
        {
            target = arg;
        }
    }

    if (target.empty())
    {
        std::cerr << "usage: minigit worktree remove [-f] <worktree>\n";
        return 1;
    }

    Repository repo(std::filesystem::current_path());
    try
    {
        repo = Repository::discover(std::filesystem::current_path());
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    auto worktrees = get_all_worktrees(repo);
    const WorktreeInfo* matched = nullptr;

    std::filesystem::path target_abs = std::filesystem::weakly_canonical(
        std::filesystem::absolute(target));

    for (const auto& wt : worktrees)
    {
        if (wt.id == target)
        {
            matched = &wt;
            break;
        }
        std::error_code ec;
        if (!wt.path.empty() && std::filesystem::equivalent(wt.path, target_abs, ec))
        {
            matched = &wt;
            break;
        }
        if (!wt.path.empty() && wt.path.generic_string() == target_abs.generic_string())
        {
            matched = &wt;
            break;
        }
    }

    if (!matched)
    {
        std::cerr << "fatal: '" << target << "' is not a working tree\n";
        return 1;
    }

    if (matched->is_main)
    {
        std::cerr << "fatal: '" << target << "' is a main working tree\n";
        return 1;
    }

    if (matched->is_locked && !force)
    {
        std::cerr << "fatal: '" << target << "' is locked"
                  << (matched->lock_reason.empty() ? "" : ": " + matched->lock_reason) << '\n';
        return 1;
    }

    if (!force && is_worktree_dirty(*matched, repo))
    {
        std::cerr << "fatal: '" << target << "' contains modified or untracked files, use --force to delete it\n";
        return 1;
    }

    std::error_code ec;
    if (std::filesystem::exists(matched->path))
    {
        std::filesystem::remove_all(matched->path, ec);
    }
    if (std::filesystem::exists(matched->git_dir))
    {
        std::filesystem::remove_all(matched->git_dir, ec);
    }

    return 0;
}

int worktree_prune(int argc, char const* argv[])
{
    bool dry_run = false;
    bool verbose = false;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-n" || arg == "--dry-run")
        {
            dry_run = true;
        }
        else if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
        else if (arg.rfind("-", 0) == 0)
        {
            std::cerr << "fatal: unknown switch '" << arg << "'\n";
            return 1;
        }
    }

    Repository repo(std::filesystem::current_path());
    try
    {
        repo = Repository::discover(std::filesystem::current_path());
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    const auto wts_dir = repo.common_dir() / "worktrees";
    if (!std::filesystem::exists(wts_dir))
        return 0;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(wts_dir, ec))
    {
        if (!entry.is_directory())
            continue;

        const auto locked_file = entry.path() / "locked";
        if (std::filesystem::exists(locked_file))
            continue;

        const auto gitdir_file = entry.path() / "gitdir";
        bool prunable = false;
        std::string reason;

        if (!std::filesystem::exists(gitdir_file))
        {
            prunable = true;
            reason = "gitdir file does not exist";
        }
        else
        {
            std::string line = read_trimmed(gitdir_file);
            std::filesystem::path tg(line);
            std::filesystem::path wt_root = tg.parent_path();
            if (!std::filesystem::exists(tg) || !std::filesystem::exists(wt_root))
            {
                prunable = true;
                reason = "gitdir file points to non-existent location";
            }
        }

        if (prunable)
        {
            std::string id = entry.path().filename().string();
            if (dry_run)
            {
                std::cout << "Removing worktrees/" << id << ": " << reason << '\n';
            }
            else
            {
                std::filesystem::remove_all(entry.path(), ec);
                if (verbose)
                {
                    std::cout << "Removing worktrees/" << id << ": " << reason << '\n';
                }
            }
        }
    }

    return 0;
}

int worktree_lock(int argc, char const* argv[])
{
    std::string reason;
    std::string target;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--reason" && i + 1 < argc)
        {
            reason = argv[++i];
        }
        else if (arg.rfind("--reason=", 0) == 0)
        {
            reason = arg.substr(9);
        }
        else if (arg.rfind("-", 0) == 0)
        {
            std::cerr << "fatal: unknown switch '" << arg << "'\n";
            return 1;
        }
        else
        {
            target = arg;
        }
    }

    if (target.empty())
    {
        std::cerr << "usage: minigit worktree lock [--reason <string>] <worktree>\n";
        return 1;
    }

    Repository repo(std::filesystem::current_path());
    try
    {
        repo = Repository::discover(std::filesystem::current_path());
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    auto worktrees = get_all_worktrees(repo);
    const WorktreeInfo* matched = nullptr;
    std::filesystem::path target_abs = std::filesystem::weakly_canonical(
        std::filesystem::absolute(target));

    for (const auto& wt : worktrees)
    {
        if (wt.id == target)
        {
            matched = &wt;
            break;
        }
        std::error_code ec;
        if (!wt.path.empty() && std::filesystem::equivalent(wt.path, target_abs, ec))
        {
            matched = &wt;
            break;
        }
    }

    if (!matched)
    {
        std::cerr << "fatal: '" << target << "' is not a working tree\n";
        return 1;
    }

    if (matched->is_main)
    {
        std::cerr << "fatal: cannot lock main working tree\n";
        return 1;
    }

    if (matched->is_locked)
    {
        std::cerr << "fatal: '" << target << "' is already locked\n";
        return 1;
    }

    std::ofstream lf(matched->git_dir / "locked", std::ios::trunc);
    if (!lf)
    {
        std::cerr << "fatal: could not create lock file\n";
        return 1;
    }
    if (!reason.empty())
        lf << reason << '\n';

    return 0;
}

int worktree_unlock(int argc, char const* argv[])
{
    std::string target;
    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.rfind("-", 0) == 0)
        {
            std::cerr << "fatal: unknown switch '" << arg << "'\n";
            return 1;
        }
        else
        {
            target = arg;
        }
    }

    if (target.empty())
    {
        std::cerr << "usage: minigit worktree unlock <worktree>\n";
        return 1;
    }

    Repository repo(std::filesystem::current_path());
    try
    {
        repo = Repository::discover(std::filesystem::current_path());
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    auto worktrees = get_all_worktrees(repo);
    const WorktreeInfo* matched = nullptr;
    std::filesystem::path target_abs = std::filesystem::weakly_canonical(
        std::filesystem::absolute(target));

    for (const auto& wt : worktrees)
    {
        if (wt.id == target)
        {
            matched = &wt;
            break;
        }
        std::error_code ec;
        if (!wt.path.empty() && std::filesystem::equivalent(wt.path, target_abs, ec))
        {
            matched = &wt;
            break;
        }
    }

    if (!matched)
    {
        std::cerr << "fatal: '" << target << "' is not a working tree\n";
        return 1;
    }

    if (matched->is_main)
    {
        std::cerr << "fatal: cannot unlock main working tree\n";
        return 1;
    }

    if (!matched->is_locked)
    {
        std::cerr << "fatal: '" << target << "' is not locked\n";
        return 1;
    }

    std::error_code ec;
    std::filesystem::remove(matched->git_dir / "locked", ec);
    return 0;
}

int worktree_move(int argc, char const* argv[])
{
    std::string target;
    std::string new_path_str;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.rfind("-", 0) == 0)
        {
            std::cerr << "fatal: unknown switch '" << arg << "'\n";
            return 1;
        }
        else
        {
            if (target.empty())
                target = arg;
            else if (new_path_str.empty())
                new_path_str = arg;
            else
            {
                std::cerr << "usage: minigit worktree move <worktree> <new-path>\n";
                return 1;
            }
        }
    }

    if (target.empty() || new_path_str.empty())
    {
        std::cerr << "usage: minigit worktree move <worktree> <new-path>\n";
        return 1;
    }

    Repository repo(std::filesystem::current_path());
    try
    {
        repo = Repository::discover(std::filesystem::current_path());
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    auto worktrees = get_all_worktrees(repo);
    const WorktreeInfo* matched = nullptr;
    std::filesystem::path target_abs = std::filesystem::weakly_canonical(
        std::filesystem::absolute(target));

    for (const auto& wt : worktrees)
    {
        if (wt.id == target)
        {
            matched = &wt;
            break;
        }
        std::error_code ec;
        if (!wt.path.empty() && std::filesystem::equivalent(wt.path, target_abs, ec))
        {
            matched = &wt;
            break;
        }
    }

    if (!matched)
    {
        std::cerr << "fatal: '" << target << "' is not a working tree\n";
        return 1;
    }

    if (matched->is_main)
    {
        std::cerr << "fatal: cannot move main working tree\n";
        return 1;
    }

    std::filesystem::path new_abs = std::filesystem::weakly_canonical(
        std::filesystem::absolute(new_path_str));

    if (std::filesystem::exists(new_abs))
    {
        std::cerr << "fatal: '" << new_path_str << "' already exists\n";
        return 1;
    }

    std::error_code ec;
    std::filesystem::rename(matched->path, new_abs, ec);
    if (ec)
    {
        std::cerr << "fatal: could not move working tree: " << ec.message() << '\n';
        return 1;
    }

    // Update gitdir in worktree metadata
    {
        std::ofstream gf(matched->git_dir / "gitdir", std::ios::trunc);
        gf << (new_abs / ".minigit").generic_string() << '\n';
    }

    // Update .minigit in moved worktree directory
    {
        std::ofstream mf(new_abs / ".minigit", std::ios::trunc);
        mf << "gitdir: " << matched->git_dir.generic_string() << '\n';
    }

    return 0;
}

int worktree_command(int argc, char const* argv[])
{
    if (argc < 3)
    {
        std::cerr << "usage: minigit worktree <subcommand> [<options>]\n\n"
                  << "Subcommands:\n"
                  << "    add      Add a new working tree\n"
                  << "    list     List working trees\n"
                  << "    remove   Remove a working tree\n"
                  << "    prune    Prune working tree information\n"
                  << "    lock     Lock a working tree\n"
                  << "    unlock   Unlock a working tree\n"
                  << "    move     Move a working tree to a new location\n";
        return 1;
    }

    using WorktreeHandler = int (*)(int, char const*[]);
    static const std::unordered_map<std::string, WorktreeHandler> handlers = {
        {"add",    worktree_add},
        {"list",   worktree_list},
        {"remove", worktree_remove},
        {"prune",  worktree_prune},
        {"lock",   worktree_lock},
        {"unlock", worktree_unlock},
        {"move",   worktree_move}
    };

    const std::string subcmd = argv[2];
    const auto it = handlers.find(subcmd);
    if (it != handlers.end())
        return it->second(argc, argv);

    std::cerr << "fatal: unknown subcommand: '" << subcmd << "'\n";
    return 1;
}

} // namespace minigit::worktree
