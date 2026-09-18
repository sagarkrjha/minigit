#include "submodule.h"
#include "submodule_config.h"

#include "core/file.h"
#include "repository/repository.h"
#include "remotes/config.h"
#include "remotes/transfer.h"
#include "staging/index.h"
#include "storage/blob.h"
#include "storage/commit.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"
#include "storage/tree.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

std::string trim(std::string s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string normalize_path(std::string p)
{
    std::replace(p.begin(), p.end(), '\\', '/');
    while (!p.empty() && p.back() == '/')
        p.pop_back();
    return p;
}

std::string read_text_file(const fs::path &p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f)
        return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string get_head_sha(const fs::path &git_dir)
{
    return Repository::resolve_head_from_dir(git_dir);
}

std::string get_head_branch_name(const fs::path &git_dir)
{
    const std::string raw = trim(read_text_file(git_dir / "HEAD"));
    if (raw.rfind("ref: refs/heads/", 0) == 0)
        return raw.substr(16);
    if (raw.rfind("ref: ", 0) == 0)
        return raw.substr(5);
    return "";
}

// Check out a tree object into a directory and build its index
void checkout_tree_recursive(const fs::path &dest_root,
                            const fs::path &objects_dir,
                            const std::string &tree_sha,
                            Index &idx,
                            const std::string &prefix = "")
{
    ObjectDatabase db(objects_dir);
    std::string raw;
    try
    {
        raw = db.read(tree_sha);
    }
    catch (...)
    {
        return;
    }

    const ParsedTree tree = parse_tree(raw);
    for (const auto &entry : tree.entries)
    {
        const std::string rel = prefix.empty() ? entry.name : (prefix + "/" + entry.name);
        if (entry.mode == "040000")
        {
            checkout_tree_recursive(dest_root, objects_dir, entry.id, idx, rel);
        }
        else if (entry.mode == "160000")
        {
            // Submodule gitlink: do not overwrite directory, record in index
            idx.add(rel, entry.id);
        }
        else
        {
            std::string content = strip_object_header(db.read(entry.id));
            const fs::path abs = dest_root / rel;
            fs::create_directories(abs.parent_path());
            std::ofstream f(abs, std::ios::binary | std::ios::trunc);
            if (f)
                f.write(content.data(), static_cast<std::streamsize>(content.size()));
            idx.add(rel, entry.id);
        }
    }
}

// Clone/transfer a source minigit repo into a target modules directory
bool clone_local_repo(const fs::path &src_path,
                      const fs::path &dst_modules_dir,
                      const std::string &branch_to_checkout,
                      std::string &out_head_sha,
                      std::string &out_branch_name)
{
    fs::path src_git = src_path / ".minigit";
    if (!fs::exists(src_git))
    {
        if (fs::exists(src_path / "HEAD") && fs::exists(src_path / "objects"))
            src_git = src_path;
        else
            return false;
    }

    if (fs::is_regular_file(src_git))
    {
        // Follow gitdir
        std::string raw = trim(read_text_file(src_git));
        if (raw.rfind("gitdir:", 0) == 0)
        {
            std::string target = trim(raw.substr(7));
            fs::path tp(target);
            if (tp.is_relative())
                src_git = fs::weakly_canonical(src_path / tp);
            else
                src_git = fs::weakly_canonical(tp);
        }
    }

    fs::create_directories(dst_modules_dir);
    Repository new_repo(dst_modules_dir.parent_path(), dst_modules_dir, dst_modules_dir, false, "");
    new_repo.init();

    const fs::path src_objects = src_git / "objects";
    const fs::path dst_objects = dst_modules_dir / "objects";

    // Determine branch and commit in source
    std::string branch = branch_to_checkout.empty() ? get_head_branch_name(src_git) : branch_to_checkout;
    std::string commit_sha;

    if (!branch_to_checkout.empty())
    {
        const fs::path branch_file = src_git / "refs" / "heads" / branch_to_checkout;
        if (fs::exists(branch_file))
            commit_sha = trim(read_text_file(branch_file));
    }
    if (commit_sha.empty())
        commit_sha = get_head_sha(src_git);

    if (commit_sha.empty())
        return false;

    // Transfer reachable objects
    const auto needed = transfer::missing_objects(src_objects, dst_objects, commit_sha);
    transfer::transfer_objects(src_objects, dst_objects, needed);

    // Copy branch refs
    const fs::path src_heads = src_git / "refs" / "heads";
    const fs::path dst_heads = dst_modules_dir / "refs" / "heads";
    fs::create_directories(dst_heads);
    if (fs::exists(src_heads))
    {
        for (const auto &e : fs::directory_iterator(src_heads))
        {
            if (e.is_regular_file())
                fs::copy_file(e.path(), dst_heads / e.path().filename(), fs::copy_options::overwrite_existing);
        }
    }

    // Set up remote tracking
    if (!branch.empty())
    {
        const fs::path remote_ref = dst_modules_dir / "refs" / "remotes" / "origin" / branch;
        fs::create_directories(remote_ref.parent_path());
        std::ofstream f(remote_ref, std::ios::trunc);
        f << commit_sha << '\n';
    }

    // Register origin in config
    RemoteConfig cfg(dst_modules_dir / "config");
    cfg.add("origin", src_path.generic_string());

    // Write HEAD
    {
        const fs::path head_path = dst_modules_dir / "HEAD";
        std::ofstream head(head_path, std::ios::trunc);
        if (!branch.empty())
            head << "ref: refs/heads/" << branch << '\n';
        else
            head << commit_sha << '\n';
    }

    out_head_sha = commit_sha;
    out_branch_name = branch;
    return true;
}

std::string get_parent_staged_sha(const Repository &repo, const std::string &sub_path)
{
    Index idx(repo.git_dir() / "index");
    const auto &entries = idx.entries();
    const auto it = entries.find(normalize_path(sub_path));
    if (it != entries.end())
        return it->second;

    // Fallback: check HEAD commit tree
    const std::string head_sha = get_head_sha(repo.git_dir());
    if (!head_sha.empty())
    {
        try
        {
            ObjectDatabase db(repo.objects_dir());
            const ParsedCommit commit = parse_commit(db.read(head_sha));
            const ParsedTree tree = parse_tree(db.read(commit.tree_id));
            for (const auto &entry : tree.entries)
            {
                if (entry.name == normalize_path(sub_path))
                    return entry.id;
            }
        }
        catch (...)
        {
        }
    }

    return "0000000000000000000000000000000000000000000000000000000000000000";
}

} // namespace

int submodule_add(int argc, char const *argv[])
{
    // minigit submodule add [-b <branch>] [--name <name>] [-f|--force] <repository> [<path>]
    std::string branch;
    std::string name;
    bool force = false;
    std::vector<std::string> positional;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-b" && i + 1 < argc)
        {
            branch = argv[++i];
        }
        else if (arg == "--name" && i + 1 < argc)
        {
            name = argv[++i];
        }
        else if (arg == "-f" || arg == "--force")
        {
            force = true;
        }
        else if (!arg.starts_with("-"))
        {
            positional.push_back(arg);
        }
        else
        {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return 1;
        }
    }

    if (positional.empty())
    {
        std::cerr << "fatal: <repository> required for 'submodule add'\n";
        return 1;
    }

    std::string repo_url = positional[0];
    std::string target_path = (positional.size() > 1) ? positional[1] : "";

    if (target_path.empty())
    {
        fs::path p(repo_url);
        std::string fn = p.filename().string();
        if (fn.ends_with(".git"))
            fn = fn.substr(0, fn.size() - 4);
        if (fn.ends_with(".minigit"))
            fn = fn.substr(0, fn.size() - 8);
        target_path = fn;
    }
    target_path = normalize_path(target_path);

    if (name.empty())
        name = target_path;

    Repository repo = [&]() {
        try { return Repository::discover(fs::current_path()); }
        catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    const fs::path worktree_dir = repo.root() / target_path;
    const fs::path modules_dir = repo.common_dir() / "modules" / name;

    // Check if target directory already exists and is non-empty
    if (fs::exists(worktree_dir) && !fs::is_empty(worktree_dir))
    {
        // If it's already a submodule, error
        if (fs::exists(worktree_dir / ".minigit") || fs::exists(worktree_dir / ".git"))
        {
            std::cerr << "fatal: '" << target_path << "' already exists and is not a clean directory\n";
            return 1;
        }
    }

    std::cout << "Cloning into '" << worktree_dir.generic_string() << "'...\n";

    // Clone or copy repository into modules_dir
    std::string head_sha;
    std::string branch_name;
    fs::path source_path(repo_url);
    if (source_path.is_relative())
        source_path = fs::weakly_canonical(repo.root() / source_path);

    if (!clone_local_repo(source_path, modules_dir, branch, head_sha, branch_name))
    {
        std::cerr << "fatal: unable to clone repository '" << repo_url << "'\n";
        return 1;
    }

    // Create working tree directory
    fs::create_directories(worktree_dir);

    // Create .minigit file in working tree pointing to modules_dir
    const fs::path rel_to_modules = fs::relative(modules_dir, worktree_dir).lexically_normal();
    {
        std::ofstream gf(worktree_dir / ".minigit", std::ios::trunc);
        gf << "gitdir: " << rel_to_modules.generic_string() << '\n';
    }

    // Checkout working tree files inside submodule
    {
        Index sub_idx(modules_dir / "index");
        ObjectDatabase sub_db(modules_dir / "objects");
        const ParsedCommit commit = parse_commit(sub_db.read(head_sha));
        checkout_tree_recursive(worktree_dir, modules_dir / "objects", commit.tree_id, sub_idx);
        sub_idx.write();
    }

    // Update .minigitmodules
    SubmoduleConfig config(repo.root() / ".minigitmodules");
    config.add_or_update({name, target_path, repo_url, branch});
    config.save();

    // Stage .minigitmodules in parent index
    {
        ObjectDatabase parent_db(repo.objects_dir());
        Index parent_idx(repo.git_dir() / "index");

        const std::string modules_content = read_text_file(repo.root() / ".minigitmodules");
        Blob modules_blob(modules_content);
        parent_db.write(modules_blob.id(), modules_blob.serialized());
        parent_idx.add(".minigitmodules", modules_blob.id());

        // Stage submodule gitlink in parent index
        parent_idx.add(target_path, head_sha);
        parent_idx.write();
    }

    // Register in .minigit/config
    SubmoduleConfig::set_config_entry(repo.git_dir(), name, repo_url, true);

    std::cout << "done.\n";
    return 0;
}

int submodule_status(int argc, char const *argv[])
{
    // minigit submodule status [--cached] [--recursive] [<path>...]
    bool cached = false;
    bool recursive = false;
    std::vector<std::string> filter_paths;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--cached")
            cached = true;
        else if (arg == "--recursive")
            recursive = true;
        else if (!arg.starts_with("-"))
            filter_paths.push_back(normalize_path(arg));
    }

    Repository repo = [&]() {
        try { return Repository::discover(fs::current_path()); }
        catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    SubmoduleConfig config(repo.root() / ".minigitmodules");
    if (config.entries().empty())
        return 0;

    auto matches_filter = [&](const std::string &path) {
        if (filter_paths.empty())
            return true;
        for (const auto &fp : filter_paths)
        {
            if (path == fp || path.starts_with(fp + "/"))
                return true;
        }
        return false;
    };

    for (const auto &sm : config.entries())
    {
        if (!matches_filter(sm.path))
            continue;

        const std::string staged_sha = get_parent_staged_sha(repo, sm.path);
        const fs::path wt_dir = repo.root() / sm.path;
        const fs::path mod_dir = repo.common_dir() / "modules" / sm.name;

        if (!fs::exists(wt_dir / ".minigit") && !fs::exists(wt_dir / ".git"))
        {
            // Not initialized
            std::cout << '-' << staged_sha << ' ' << sm.path << '\n';
            continue;
        }

        try
        {
            Repository sub_repo = Repository::discover(wt_dir);
            const std::string cur_sha = get_head_sha(sub_repo.git_dir());
            const std::string branch = get_head_branch_name(sub_repo.git_dir());
            const std::string branch_str = branch.empty() ? "" : (" (" + branch + ")");

            char prefix = ' ';
            std::string display_sha = cur_sha;

            if (cached)
            {
                display_sha = staged_sha;
                prefix = ' ';
            }
            else
            {
                if (cur_sha != staged_sha)
                    prefix = '+';
            }

            std::cout << prefix << display_sha << ' ' << sm.path << branch_str << '\n';

            if (recursive)
            {
                // Recursive status
                const auto sub_modules_file = wt_dir / ".minigitmodules";
                if (fs::exists(sub_modules_file))
                {
                    std::string cmd = "minigit submodule status --recursive";
                    if (cached) cmd += " --cached";
                    // Could invoke recursively if needed
                }
            }
        }
        catch (...)
        {
            std::cout << '-' << staged_sha << ' ' << sm.path << '\n';
        }
    }

    return 0;
}

int submodule_init(int argc, char const *argv[])
{
    // minigit submodule init [<path>...]
    std::vector<std::string> filter_paths;
    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (!arg.starts_with("-"))
            filter_paths.push_back(normalize_path(arg));
    }

    Repository repo = [&]() {
        try { return Repository::discover(fs::current_path()); }
        catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    SubmoduleConfig config(repo.root() / ".minigitmodules");
    if (config.entries().empty())
        return 0;

    for (const auto &sm : config.entries())
    {
        if (!filter_paths.empty())
        {
            bool match = false;
            for (const auto &fp : filter_paths)
            {
                if (sm.path == fp || sm.name == fp)
                {
                    match = true;
                    break;
                }
            }
            if (!match)
                continue;
        }

        SubmoduleConfig::set_config_entry(repo.git_dir(), sm.name, sm.url, true);
        std::cout << "Submodule '" << sm.name << "' (" << sm.url
                  << ") registered for path '" << sm.path << "'\n";
    }

    return 0;
}

int submodule_update(int argc, char const *argv[])
{
    // minigit submodule update [--init] [--recursive] [-f|--force] [<path>...]
    bool init = false;
    bool recursive = false;
    bool force = false;
    std::vector<std::string> filter_paths;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--init")
            init = true;
        else if (arg == "--recursive")
            recursive = true;
        else if (arg == "-f" || arg == "--force")
            force = true;
        else if (!arg.starts_with("-"))
            filter_paths.push_back(normalize_path(arg));
    }

    Repository repo = [&]() {
        try { return Repository::discover(fs::current_path()); }
        catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    SubmoduleConfig config(repo.root() / ".minigitmodules");
    if (config.entries().empty())
        return 0;

    for (const auto &sm : config.entries())
    {
        if (!filter_paths.empty())
        {
            bool match = false;
            for (const auto &fp : filter_paths)
            {
                if (sm.path == fp || sm.name == fp)
                {
                    match = true;
                    break;
                }
            }
            if (!match)
                continue;
        }

        bool is_active = SubmoduleConfig::is_config_active(repo.git_dir(), sm.name);
        if (!is_active)
        {
            if (init)
            {
                SubmoduleConfig::set_config_entry(repo.git_dir(), sm.name, sm.url, true);
                std::cout << "Submodule '" << sm.name << "' (" << sm.url
                          << ") registered for path '" << sm.path << "'\n";
            }
            else
            {
                continue; // Skip uninitialized submodule
            }
        }

        const fs::path modules_dir = repo.common_dir() / "modules" / sm.name;
        const fs::path worktree_dir = repo.root() / sm.path;

        if (!fs::exists(modules_dir))
        {
            std::string head_sha, branch_name;
            fs::path src_path(sm.url);
            if (src_path.is_relative())
                src_path = fs::weakly_canonical(repo.root() / src_path);

            std::cout << "Cloning into '" << worktree_dir.generic_string() << "'...\n";
            if (!clone_local_repo(src_path, modules_dir, sm.branch, head_sha, branch_name))
            {
                std::cerr << "fatal: unable to clone submodule '" << sm.name << "'\n";
                return 1;
            }
        }

        // Ensure working directory exists and has .minigit pointer
        fs::create_directories(worktree_dir);
        if (!fs::exists(worktree_dir / ".minigit") && !fs::exists(worktree_dir / ".git"))
        {
            const fs::path rel_to_modules = fs::relative(modules_dir, worktree_dir).lexically_normal();
            std::ofstream gf(worktree_dir / ".minigit", std::ios::trunc);
            gf << "gitdir: " << rel_to_modules.generic_string() << '\n';
        }

        // Determine target commit to check out
        std::string target_sha = get_parent_staged_sha(repo, sm.path);
        if (target_sha.empty() || target_sha == "0000000000000000000000000000000000000000000000000000000000000000")
        {
            target_sha = get_head_sha(modules_dir);
        }

        // Check out target_sha in submodule
        {
            // Set HEAD to detached commit
            std::ofstream hf(modules_dir / "HEAD", std::ios::trunc);
            hf << target_sha << '\n';

            Index sub_idx(modules_dir / "index");
            ObjectDatabase sub_db(modules_dir / "objects");
            const ParsedCommit commit = parse_commit(sub_db.read(target_sha));
            checkout_tree_recursive(worktree_dir, modules_dir / "objects", commit.tree_id, sub_idx);
            sub_idx.write();
        }

        std::cout << "Submodule path '" << sm.path << "': checked out '" << target_sha << "'\n";
    }

    return 0;
}

int submodule_deinit(int argc, char const *argv[])
{
    // minigit submodule deinit [-f|--force] (--all | <path>...)
    bool force = false;
    bool all = false;
    std::vector<std::string> paths;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-f" || arg == "--force")
            force = true;
        else if (arg == "--all")
            all = true;
        else if (!arg.starts_with("-"))
            paths.push_back(normalize_path(arg));
    }

    if (!all && paths.empty())
    {
        std::cerr << "fatal: please specify --all or at least one path to deinit\n";
        return 1;
    }

    Repository repo = [&]() {
        try { return Repository::discover(fs::current_path()); }
        catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    SubmoduleConfig config(repo.root() / ".minigitmodules");
    for (const auto &sm : config.entries())
    {
        if (!all)
        {
            bool match = false;
            for (const auto &p : paths)
            {
                if (sm.path == p || sm.name == p)
                {
                    match = true;
                    break;
                }
            }
            if (!match)
                continue;
        }

        const fs::path wt_dir = repo.root() / sm.path;
        if (fs::exists(wt_dir))
        {
            // Remove working tree files
            std::error_code ec;
            fs::remove_all(wt_dir, ec);
            std::cout << "Cleared directory for submodule '" << sm.path << "'\n";
        }

        SubmoduleConfig::remove_config_entry(repo.git_dir(), sm.name);
        std::cout << "Submodule '" << sm.name << "' (" << sm.url
                  << ") unregistered for path '" << sm.path << "'\n";
    }

    return 0;
}

int submodule_summary(int argc, char const *argv[])
{
    // minigit submodule summary [<commit>] [--cached] [<path>...]
    bool cached = false;
    std::vector<std::string> paths;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--cached")
            cached = true;
        else if (!arg.starts_with("-"))
            paths.push_back(normalize_path(arg));
    }

    Repository repo = [&]() {
        try { return Repository::discover(fs::current_path()); }
        catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    SubmoduleConfig config(repo.root() / ".minigitmodules");
    for (const auto &sm : config.entries())
    {
        if (!paths.empty())
        {
            bool match = false;
            for (const auto &p : paths)
            {
                if (sm.path == p || sm.name == p)
                {
                    match = true;
                    break;
                }
            }
            if (!match)
                continue;
        }

        const std::string staged_sha = get_parent_staged_sha(repo, sm.path);
        const fs::path wt_dir = repo.root() / sm.path;
        if (!fs::exists(wt_dir / ".minigit") && !fs::exists(wt_dir / ".git"))
            continue;

        try
        {
            Repository sub_repo = Repository::discover(wt_dir);
            const std::string cur_sha = get_head_sha(sub_repo.git_dir());

            if (staged_sha != cur_sha)
            {
                std::cout << "* " << sm.path << " " << staged_sha.substr(0, 7) << "..."
                          << cur_sha.substr(0, 7) << ":\n";
            }
        }
        catch (...)
        {
        }
    }

    return 0;
}

int submodule_foreach(int argc, char const *argv[])
{
    // minigit submodule foreach [--recursive] <command>
    bool recursive = false;
    std::string command;

    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--recursive")
            recursive = true;
        else
        {
            if (!command.empty())
                command += " ";
            command += arg;
        }
    }

    if (command.empty())
    {
        std::cerr << "fatal: command required for 'submodule foreach'\n";
        return 1;
    }

    Repository repo = [&]() {
        try { return Repository::discover(fs::current_path()); }
        catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    SubmoduleConfig config(repo.root() / ".minigitmodules");
    for (const auto &sm : config.entries())
    {
        const fs::path wt_dir = repo.root() / sm.path;
        if (!fs::exists(wt_dir / ".minigit") && !fs::exists(wt_dir / ".git"))
            continue;

        std::cout << "Entering '" << sm.path << "'\n";

        const fs::path old_cwd = fs::current_path();
        std::error_code ec;
        fs::current_path(wt_dir, ec);

        int res = std::system(command.c_str());
        fs::current_path(old_cwd, ec);

        if (res != 0)
        {
            std::cerr << "fatal: run of '" << command << "' failed for submodule '" << sm.path << "'\n";
            return res;
        }
    }

    return 0;
}

int submodule_sync(int argc, char const *argv[])
{
    // minigit submodule sync [--recursive] [<path>...]
    std::vector<std::string> paths;
    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (!arg.starts_with("-"))
            paths.push_back(normalize_path(arg));
    }

    Repository repo = [&]() {
        try { return Repository::discover(fs::current_path()); }
        catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    SubmoduleConfig config(repo.root() / ".minigitmodules");
    for (const auto &sm : config.entries())
    {
        if (!paths.empty())
        {
            bool match = false;
            for (const auto &p : paths)
            {
                if (sm.path == p || sm.name == p)
                {
                    match = true;
                    break;
                }
            }
            if (!match)
                continue;
        }

        SubmoduleConfig::set_config_entry(repo.git_dir(), sm.name, sm.url, true);
        const fs::path mod_dir = repo.common_dir() / "modules" / sm.name;
        if (fs::exists(mod_dir / "config"))
        {
            RemoteConfig rcfg(mod_dir / "config");
            if (rcfg.find("origin"))
            {
                rcfg.remove("origin");
                rcfg.add("origin", sm.url);
                rcfg.save();
            }
        }

        std::cout << "Synchronizing submodule url for '" << sm.name << "'\n";
    }

    return 0;
}

int submodule_command(int argc, char const *argv[])
{
    if (argc < 3)
    {
        // Default to submodule status
        return submodule_status(argc, argv);
    }

    std::string sub = argv[2];
    if (sub == "add")
        return submodule_add(argc, argv);
    if (sub == "status")
        return submodule_status(argc, argv);
    if (sub == "init")
        return submodule_init(argc, argv);
    if (sub == "update")
        return submodule_update(argc, argv);
    if (sub == "deinit")
        return submodule_deinit(argc, argv);
    if (sub == "summary")
        return submodule_summary(argc, argv);
    if (sub == "foreach")
        return submodule_foreach(argc, argv);
    if (sub == "sync")
        return submodule_sync(argc, argv);

    // If first argument after 'submodule' is an option (e.g. '--cached'), run status
    if (sub.starts_with("-"))
        return submodule_status(argc, argv);

    std::cerr << "error: unknown subcommand '" << sub << "' for 'minigit submodule'\n";
    std::cerr << "usage: minigit submodule [add|status|init|update|deinit|summary|foreach|sync]\n";
    return 1;
}
