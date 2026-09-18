#include "switch_branch.h"
#include "branch.h"
#include "checkout.h"

#include "repository/repository.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

static std::string read_text(const fs::path &path)
{
    std::ifstream f(path);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string trim_trailing(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

void switch_command(const std::string &branch, bool create)
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    const fs::path branch_ref = repo.common_dir() / "refs" / "heads" / branch;

    if (create)
    {
        // Create the branch first, then check it out.
        if (fs::exists(branch_ref))
        {
            std::cerr << "fatal: a branch named '" << branch
                      << "' already exists\n";
            std::exit(1);
        }

        // Read current HEAD commit SHA.
        const std::string head_sha = Repository::resolve_head_from_dir(repo.git_dir());

        if (head_sha.empty())
        {
            std::cerr << "fatal: not a valid object name: 'HEAD' "
                         "(no commits yet)\n";
            std::exit(1);
        }

        // Write new branch ref.
        fs::create_directories(branch_ref.parent_path());
        std::ofstream f(branch_ref);
        if (!f)
        {
            std::cerr << "error: could not create branch ref\n";
            std::exit(1);
        }
        f << head_sha << '\n';
    }
    else
    {
        // Branch must already exist.
        if (!fs::exists(branch_ref))
        {
            std::cerr << "error: pathspec '" << branch
                      << "' did not match any branch known to minigit\n";
            std::exit(1);
        }

        auto match = repo.find_branch_worktree(branch);
        if (match.is_checked_out)
        {
            if (match.is_current_worktree)
            {
                std::cout << "Already on '" << branch << "'\n";
                return;
            }
            else
            {
                std::cerr << "fatal: '" << branch << "' is already checked out at '"
                          << match.worktree_path.generic_string() << "'\n";
                std::exit(1);
            }
        }
    }

    // Delegate actual checkout (restores tree + updates HEAD).
    checkout_command(branch);
}
