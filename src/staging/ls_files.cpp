#include "ls_files.h"

#include "ignore.h"
#include "index.h"
#include "repository/repository.h"
#include "storage/blob.h"
#include "submodule/submodule_config.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string hash_file(const fs::path& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return {};

    std::string content{
        std::istreambuf_iterator<char>{f},
        std::istreambuf_iterator<char>{}};

    Blob blob(std::move(content));
    return blob.id();
}

bool matches_filters(const std::string& rel_path, const std::vector<std::string>& filters)
{
    if (filters.empty())
        return true;

    for (const auto& f : filters)
    {
        std::string norm = f;
        while (norm.size() > 1 && (norm.back() == '/' || norm.back() == '\\'))
            norm.pop_back();

        if (norm == "." || norm.empty())
            return true;

        if (rel_path == norm || rel_path.rfind(norm + "/", 0) == 0)
            return true;
    }

    return false;
}

std::set<std::string> scan_working_tree_files(const fs::path& root, const IgnoreRules& ignore_rules)
{
    std::set<std::string> result;
    std::error_code ec;

    for (const auto& entry : fs::recursive_directory_iterator(root, ec))
    {
        if (!entry.is_regular_file())
            continue;

        const auto rel = fs::relative(entry.path(), root);
        const std::string rel_str = rel.generic_string();

        if (rel_str == ".minigit" || rel_str.rfind(".minigit/", 0) == 0 ||
            rel_str == ".git" || rel_str.rfind(".git/", 0) == 0)
            continue;

        if (rel_str == ".minigitignore")
            continue;

        // Skip files inside submodules
        bool in_submodule = false;
        fs::path p_scan = entry.path().parent_path();
        while (p_scan != root && p_scan.has_relative_path())
        {
            if (fs::exists(p_scan / ".minigit") || fs::exists(p_scan / ".git"))
            {
                in_submodule = true;
                break;
            }
            p_scan = p_scan.parent_path();
        }
        if (in_submodule)
            continue;

        if (ignore_rules.is_ignored(rel_str))
            continue;

        result.insert(rel_str);
    }

    return result;
}

} // namespace

LsFilesResult perform_ls_files(
    const fs::path& repo_root,
    const LsFilesOptions& options)
{
    LsFilesResult result;
    const fs::path git_dir = repo_root / ".minigit";

    if (!fs::exists(git_dir))
    {
        result.error_message = "fatal: not a git repository (or any of the parent directories): .minigit";
        return result;
    }

    Index index(git_dir / "index");
    const auto& staged = index.entries(); // path -> blob_id

    // Default mode: if no specific filtering flags (-c, -d, -m, -o) given, default to -c (cached)
    bool show_cached = options.cached;
    bool show_deleted = options.deleted;
    bool show_modified = options.modified;
    bool show_others = options.others;

    if (!show_cached && !show_deleted && !show_modified && !show_others)
    {
        show_cached = true;
    }

    std::set<std::string> matched_paths;
    std::map<std::string, std::string> staged_sorted(staged.begin(), staged.end());

    // 1. Cached / Tracked files
    if (show_cached)
    {
        for (const auto& [path, _] : staged_sorted)
        {
            if (matches_filters(path, options.paths))
                matched_paths.insert(path);
        }
    }

    // 2. Deleted files
    if (show_deleted)
    {
        for (const auto& [path, _] : staged_sorted)
        {
            if (!fs::exists(repo_root / path))
            {
                if (matches_filters(path, options.paths))
                    matched_paths.insert(path);
            }
        }
    }

    // 3. Modified files
    if (show_modified)
    {
        for (const auto& [path, blob_id] : staged_sorted)
        {
            const fs::path abs = repo_root / path;
            if (fs::exists(abs))
            {
                if (hash_file(abs) != blob_id)
                {
                    if (matches_filters(path, options.paths))
                        matched_paths.insert(path);
                }
            }
        }
    }

    // 4. Other (untracked) files
    if (show_others)
    {
        IgnoreRules ignore_rules = IgnoreRules::load(repo_root);
        auto wt_files = scan_working_tree_files(repo_root, ignore_rules);
        for (const auto& path : wt_files)
        {
            if (staged.find(path) == staged.end())
            {
                if (matches_filters(path, options.paths))
                    matched_paths.insert(path);
            }
        }
    }

    std::ostringstream out;

    for (const auto& path : matched_paths)
    {
        if (options.stage)
        {
            // Format: <mode> <sha256> <stage>\t<path>
            const auto it = staged.find(path);
            const std::string sha = (it != staged.end()) ? it->second : "0000000000000000000000000000000000000000000000000000000000000000";
            const std::string mode = SubmoduleConfig::is_submodule_path(repo_root, path) ? "160000" : "100644";
            out << mode << " " << sha << " 0\t" << path << '\n';
        }
        else
        {
            out << path << '\n';
        }
        result.entries.push_back(path);
    }

    result.output = out.str();
    result.success = true;
    return result;
}

int ls_files_command(int argc, char const *argv[])
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    LsFilesOptions options;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--stage")
            options.stage = true;
        else if (arg == "--cached")
            options.cached = true;
        else if (arg == "--deleted")
            options.deleted = true;
        else if (arg == "--modified")
            options.modified = true;
        else if (arg == "--others")
            options.others = true;
        else if (arg.rfind("--", 0) == 0)
        {
            std::cerr << "error: unknown option: " << arg << '\n';
            std::cerr << "usage: minigit ls-files [-s | --stage] [-c | --cached] [-d | --deleted] [-m | --modified] [-o | --others] [<path>...]\n";
            return 1;
        }
        else if (arg.rfind("-", 0) == 0 && arg.size() > 1)
        {
            for (size_t c = 1; c < arg.size(); ++c)
            {
                switch (arg[c])
                {
                    case 's': options.stage = true; break;
                    case 'c': options.cached = true; break;
                    case 'd': options.deleted = true; break;
                    case 'm': options.modified = true; break;
                    case 'o': options.others = true; break;
                    default:
                        std::cerr << "error: unknown switch: -" << arg[c] << '\n';
                        std::cerr << "usage: minigit ls-files [-s] [-c] [-d] [-m] [-o] [<path>...]\n";
                        return 1;
                }
            }
        }
        else
        {
            options.paths.push_back(arg);
        }
    }

    LsFilesResult res = perform_ls_files(repo.root(), options);
    if (!res.success)
    {
        std::cerr << res.error_message << '\n';
        return 1;
    }

    std::cout << res.output;
    return 0;
}
