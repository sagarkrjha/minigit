#include "add.h"

#include "core/file.h"
#include "ignore.h"
#include "index.h"
#include "storage/blob.h"
#include "storage/object_database.h"
#include "repository/repository.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

bool stage_single_file(const std::filesystem::path &abs_path,
                       const std::filesystem::path &rel_path,
                       const IgnoreRules &ignore_rules,
                       Index &index,
                       ObjectDatabase &db,
                       bool warn_on_ignored)
{
    const std::string rel_str = rel_path.generic_string();

    // Skip .minigit internals and any real .git directory.
    for (const auto &part : rel_path)
    {
        if (part == ".minigit" || part == ".git")
            return true;
    }

    // Check ignore rules.
    // If the file is already in the index the user is explicitly re-staging it.
    const bool already_staged = index.entries().count(rel_str) > 0;

    // Skip .minigitignore itself during directory recursion unless already staged or explicitly added.
    if (rel_str == ".minigitignore" && !warn_on_ignored && !already_staged)
        return true;

    if (!already_staged && ignore_rules.is_ignored(rel_str))
    {
        if (warn_on_ignored)
        {
            std::cerr << "warning: ignoring '" << rel_str
                      << "' (matched by .minigitignore)\n";
        }
        return true;
    }

    // Read file contents.
    std::string content;
    try
    {
        content = read_file(abs_path);
    }
    catch (const std::exception &e)
    {
        std::cerr << "error: unable to read '" << rel_str << "': " << e.what() << '\n';
        return false;
    }

    // Create blob and persist it.
    Blob blob(std::move(content));
    try
    {
        db.write(blob.id(), blob.serialized());
    }
    catch (const std::exception &e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return false;
    }

    // Stage the entry.
    index.add(rel_path, blob.id());
    return true;
}

} // namespace

bool add_files(const std::vector<std::string> &paths)
{
    const auto cwd = std::filesystem::current_path();

    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(cwd);
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    ObjectDatabase db(repo.git_dir() / "objects");
    Index index(repo.git_dir() / "index");

    const IgnoreRules ignore_rules = IgnoreRules::load(repo.root());

    bool had_error = false;

    for (const auto &raw_path : paths)
    {
        const std::filesystem::path abs_path =
            std::filesystem::weakly_canonical(cwd / raw_path);

        // Ensure the path is inside the repository root.
        const auto rel_path =
            std::filesystem::relative(abs_path, repo.root()).lexically_normal();

        const std::string rel_str = rel_path.generic_string();

        if (rel_str == ".." || rel_str.starts_with("../") || rel_path.string().starts_with(".."))
        {
            std::cerr << "error: '" << raw_path
                      << "' is outside repository\n";
            had_error = true;
            continue;
        }

        // Skip direct attempts to stage .minigit or .git directories.
        if (rel_str == ".minigit" || rel_str.starts_with(".minigit/") ||
            rel_str == ".git" || rel_str.starts_with(".git/"))
        {
            continue;
        }

        // Case 1: Path does not exist in working tree.
        std::error_code ec;
        if (!std::filesystem::exists(abs_path, ec))
        {
            if (index.entries().count(rel_str) > 0)
            {
                // File was deleted from working tree; stage its removal.
                index.remove(rel_path);
            }
            else
            {
                // Check if rel_str represents a directory whose tracked files were deleted.
                const std::string prefix = (rel_path == "." || rel_path.empty()) ? "" : (rel_str + "/");
                std::vector<std::string> to_remove;
                for (const auto &[staged_path, _] : index.entries())
                {
                    if (prefix.empty() || staged_path.starts_with(prefix))
                    {
                        to_remove.push_back(staged_path);
                    }
                }

                if (!to_remove.empty())
                {
                    for (const auto &p : to_remove)
                    {
                        index.remove(p);
                    }
                }
                else
                {
                    std::cerr << "error: pathspec '" << raw_path
                              << "' did not match any files\n";
                    had_error = true;
                }
            }
            continue;
        }

        // Case 2: Path is a directory (e.g. `.` or a subfolder).
        if (std::filesystem::is_directory(abs_path, ec))
        {
            const std::string dir_rel_str =
                (rel_path == "." || rel_path.empty()) ? "" : rel_str;

            // If the user specified an ignored directory directly, warn and skip.
            if (!dir_rel_str.empty() &&
                (ignore_rules.is_ignored(dir_rel_str) || ignore_rules.is_ignored(dir_rel_str + "/")))
            {
                std::cerr << "warning: ignoring '" << raw_path
                          << "' (matched by .minigitignore)\n";
                continue;
            }

            auto it = std::filesystem::recursive_directory_iterator(
                abs_path,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            auto end = std::filesystem::recursive_directory_iterator();

            while (!ec && it != end)
            {
                const auto &entry = *it;
                if (entry.is_directory(ec))
                {
                    const auto dir_rel = std::filesystem::relative(entry.path(), repo.root()).lexically_normal();
                    const std::string cur_dir_rel_str = dir_rel.generic_string();

                    // Do not descend into .minigit, .git, or ignored directories.
                    if (entry.path().filename() == ".minigit" ||
                        entry.path().filename() == ".git" ||
                        cur_dir_rel_str == ".minigit" || cur_dir_rel_str.starts_with(".minigit/") ||
                        cur_dir_rel_str == ".git" || cur_dir_rel_str.starts_with(".git/") ||
                        ignore_rules.is_ignored(cur_dir_rel_str) ||
                        ignore_rules.is_ignored(cur_dir_rel_str + "/"))
                    {
                        it.disable_recursion_pending();
                    }
                    it.increment(ec);
                    continue;
                }

                if (entry.is_regular_file(ec))
                {
                    const auto entry_abs = entry.path();
                    const auto entry_rel = std::filesystem::relative(entry_abs, repo.root()).lexically_normal();

                    if (!stage_single_file(entry_abs, entry_rel, ignore_rules, index, db, false))
                    {
                        had_error = true;
                    }
                }

                it.increment(ec);
            }

            if (ec)
            {
                std::cerr << "error: " << ec.message() << '\n';
                had_error = true;
            }

            // Stage removal of any tracked files within this directory that were deleted on disk.
            const std::string dir_rel_prefix =
                dir_rel_str.empty() ? "" : (dir_rel_str + "/");

            std::vector<std::string> to_remove;
            for (const auto &[staged_path, _] : index.entries())
            {
                if (dir_rel_prefix.empty() || staged_path.starts_with(dir_rel_prefix))
                {
                    if (!std::filesystem::exists(repo.root() / staged_path))
                    {
                        to_remove.push_back(staged_path);
                    }
                }
            }
            for (const auto &p : to_remove)
            {
                index.remove(p);
            }

            continue;
        }

        // Case 3: Path is a regular file.
        if (std::filesystem::is_regular_file(abs_path, ec))
        {
            if (!stage_single_file(abs_path, rel_path, ignore_rules, index, db, true))
            {
                had_error = true;
            }
            continue;
        }

        // Case 4: Special file types (socket, device, etc.).
        std::cerr << "error: pathspec '" << raw_path
                  << "' did not match any files\n";
        had_error = true;
    }

    // Persist updated index only if no errors occurred.
    if (!had_error)
    {
        try
        {
            index.write();
        }
        catch (const std::exception &e)
        {
            std::cerr << "error: " << e.what() << '\n';
            return false;
        }
    }

    return !had_error;
}
