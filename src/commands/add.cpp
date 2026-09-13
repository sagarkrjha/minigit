#include "add.h"

#include "../filesystem/file.h"
#include "../ignore/ignore.h"
#include "../index/index.h"
#include "../objects/blob.h"
#include "../objects/object_database.h"
#include "../repository/repository.h"

#include <iostream>
#include <stdexcept>

void add_files(const std::vector<std::string> &paths)
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

    for (const auto& raw_path : paths)
    {
        const std::filesystem::path abs_path =
            std::filesystem::weakly_canonical(cwd / raw_path);

        // Ensure the file is inside the repository root.
        const auto rel_path =
            std::filesystem::relative(abs_path, repo.root());

        if (rel_path.string().starts_with(".."))
        {
            std::cerr << "error: '" << raw_path
                      << "' is outside repository\n";
            had_error = true;
            continue;
        }

        // Check ignore rules.
        // If the file is already in the index the user is explicitly re-staging
        // it, so we allow it through (mirrors git's behaviour).
        const std::string rel_str = rel_path.generic_string();
        const bool already_staged = index.entries().count(rel_str) > 0;
        if (!already_staged && ignore_rules.is_ignored(rel_str))
        {
            std::cerr << "warning: ignoring '" << rel_str
                      << "' (matched by .minigitignore)\n";
            continue;
        }

        // Read file contents.
        std::string content;
        try
        {
            content = read_file(abs_path);
        }
        catch (const std::exception& e)
        {
            std::cerr << "error: pathspec '" << raw_path
                      << "' did not match any files\n";
            had_error = true;
            continue;
        }

        // Create blob and persist it.
        Blob blob(std::move(content));
        try
        {
            db.write(blob.id(), blob.serialized());
        }
        catch (const std::exception& e)
        {
            std::cerr << "error: " << e.what() << '\n';
            had_error = true;
            continue;
        }

        // Stage the entry.
        index.add(rel_path, blob.id());
    }

    // Persist updated index only if at least one file was staged successfully.
    if (!had_error || index.entries().size() > 0)
    {
        try
        {
            index.write();
        }
        catch (const std::exception &e)
        {
            std::cerr << "error: " << e.what() << '\n';
        }
    }
}
