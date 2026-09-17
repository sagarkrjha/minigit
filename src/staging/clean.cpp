#include "clean.h"

#include "ignore.h"
#include "index.h"
#include "repository/repository.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool matches_path_filter(const std::string& rel_path, const std::vector<std::string>& filters)
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

void collect_untracked(
    const fs::path& repo_root,
    const fs::path& current_dir,
    const std::string& rel_dir,
    const std::unordered_set<std::string>& tracked_files,
    const IgnoreRules& ignore_rules,
    const CleanOptions& options,
    std::vector<std::string>& untracked_files,
    std::vector<std::string>& untracked_dirs)
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(current_dir, ec))
    {
        const std::string name = entry.path().filename().string();
        const std::string rel_path = rel_dir.empty() ? name : (rel_dir + "/" + name);

        // Skip internal VCS directories
        if (rel_path == ".minigit" || rel_path.rfind(".minigit/", 0) == 0 ||
            rel_path == ".git" || rel_path.rfind(".git/", 0) == 0)
        {
            continue;
        }

        // Check if ignored by .minigitignore
        if (!options.include_ignored && ignore_rules.is_ignored(rel_path))
        {
            continue;
        }

        if (entry.is_directory())
        {
            // Check if any tracked file lives under this directory subtree
            bool has_tracked = false;
            const std::string prefix = rel_path + "/";
            for (const auto& t : tracked_files)
            {
                if (t.rfind(prefix, 0) == 0)
                {
                    has_tracked = true;
                    break;
                }
            }

            if (!has_tracked)
            {
                if (options.remove_directories)
                {
                    if (matches_path_filter(rel_path, options.paths))
                    {
                        untracked_dirs.push_back(rel_path + "/");
                        continue; // entire directory handled as a unit
                    }
                }
                // If -d is not specified or filter didn't match, recurse to check untracked files inside
                collect_untracked(repo_root, entry.path(), rel_path, tracked_files,
                                  ignore_rules, options, untracked_files, untracked_dirs);
            }
            else
            {
                // Has tracked files: recurse into directory
                collect_untracked(repo_root, entry.path(), rel_path, tracked_files,
                                  ignore_rules, options, untracked_files, untracked_dirs);
            }
        }
        else if (entry.is_regular_file())
        {
            // By default .minigitignore is preserved unless -x is active
            if (rel_path == ".minigitignore" && !options.include_ignored)
                continue;

            if (tracked_files.find(rel_path) == tracked_files.end())
            {
                if (matches_path_filter(rel_path, options.paths))
                {
                    untracked_files.push_back(rel_path);
                }
            }
        }
    }
}

} // namespace

CleanResult perform_clean(
    const fs::path& repo_root,
    const CleanOptions& options)
{
    CleanResult result;

    // Safety guard: require force (-f) or dry-run (-n)
    if (!options.force && !options.dry_run)
    {
        result.error_message =
            "fatal: clean.requireForce defaults to true and neither -i, -n, nor -f given; refusing to clean";
        return result;
    }

    const fs::path git_dir = repo_root / ".minigit";
    if (!fs::exists(git_dir))
    {
        result.error_message =
            "fatal: not a git repository (or any of the parent directories): .minigit";
        return result;
    }

    Index index(git_dir / "index");
    std::unordered_set<std::string> tracked;
    for (const auto& [p, _] : index.entries())
    {
        tracked.insert(p);
    }

    IgnoreRules ignore_rules = IgnoreRules::load(repo_root);

    std::vector<std::string> untracked_files;
    std::vector<std::string> untracked_dirs;

    collect_untracked(repo_root, repo_root, "", tracked, ignore_rules, options,
                      untracked_files, untracked_dirs);

    // Sort items for deterministic reporting
    std::sort(untracked_files.begin(), untracked_files.end());
    std::sort(untracked_dirs.begin(), untracked_dirs.end());

    std::ostringstream out;

    // Process untracked files
    for (const auto& file : untracked_files)
    {
        if (options.dry_run)
        {
            out << "Would remove " << file << '\n';
            result.items_cleaned.push_back(file);
        }
        else if (options.force)
        {
            std::error_code ec;
            fs::remove(repo_root / file, ec);
            out << "Removing " << file << '\n';
            result.items_cleaned.push_back(file);
        }
    }

    // Process untracked directories
    for (const auto& dir : untracked_dirs)
    {
        // Strip trailing slash for filesystem operation
        std::string dir_no_slash = dir;
        if (!dir_no_slash.empty() && dir_no_slash.back() == '/')
            dir_no_slash.pop_back();

        if (options.dry_run)
        {
            out << "Would remove " << dir << '\n';
            result.items_cleaned.push_back(dir);
        }
        else if (options.force)
        {
            std::error_code ec;
            fs::remove_all(repo_root / dir_no_slash, ec);
            out << "Removing " << dir << '\n';
            result.items_cleaned.push_back(dir);
        }
    }

    result.output = out.str();
    result.success = true;
    return result;
}

int clean_command(int argc, char const *argv[])
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    CleanOptions options;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--force")
        {
            options.force = true;
        }
        else if (arg == "--dry-run")
        {
            options.dry_run = true;
        }
        else if (arg.rfind("--", 0) == 0)
        {
            std::cerr << "error: unknown option: " << arg << '\n';
            std::cerr << "usage: minigit clean [-f | -n] [-d] [-x] [<path>...]\n";
            return 1;
        }
        else if (arg.rfind("-", 0) == 0 && arg.size() > 1)
        {
            // Handle combined flags: e.g. -fd, -df, -nd, -x, etc.
            for (size_t c = 1; c < arg.size(); ++c)
            {
                switch (arg[c])
                {
                    case 'f':
                        options.force = true;
                        break;
                    case 'n':
                        options.dry_run = true;
                        break;
                    case 'd':
                        options.remove_directories = true;
                        break;
                    case 'x':
                        options.include_ignored = true;
                        break;
                    default:
                        std::cerr << "error: unknown switch: -" << arg[c] << '\n';
                        std::cerr << "usage: minigit clean [-f | -n] [-d] [-x] [<path>...]\n";
                        return 1;
                }
            }
        }
        else
        {
            options.paths.push_back(arg);
        }
    }

    CleanResult res = perform_clean(repo.root(), options);
    if (!res.success)
    {
        std::cerr << res.error_message << '\n';
        return 1;
    }

    std::cout << res.output;
    return 0;
}
