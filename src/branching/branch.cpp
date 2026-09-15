#include "branch.h"

#include "repository/repository.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

// Return the name of the currently checked-out branch ("" if detached HEAD).
static std::string current_branch(const fs::path &git_dir)
{
    const std::string raw = trim_trailing(read_text(git_dir / "HEAD"));
    if (raw.substr(0, 5) == "ref: ")
    {
        const std::string ref = raw.substr(5); // e.g. refs/heads/main
        const auto slash = ref.rfind('/');
        return (slash != std::string::npos) ? ref.substr(slash + 1) : ref;
    }
    return {}; // detached HEAD
}

// Resolve HEAD to a commit SHA.
static std::string resolve_head(const fs::path &git_dir)
{
    const std::string raw = trim_trailing(read_text(git_dir / "HEAD"));
    if (raw.empty()) return {};
    if (raw.substr(0, 5) == "ref: ")
        return trim_trailing(read_text(git_dir / raw.substr(5)));
    return raw;
}

void branch_command(const std::string &name, bool delete_branch)
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    const fs::path heads_dir = repo.git_dir() / "refs" / "heads";

    // -----------------------------------------------------------------------
    // No name: list branches.
    // -----------------------------------------------------------------------
    if (name.empty())
    {
        const std::string cur = current_branch(repo.git_dir());

        if (!fs::exists(heads_dir))
        {
            std::cout << "(no branches)\n";
            return;
        }

        std::vector<std::string> branches;
        for (const auto &entry : fs::directory_iterator(heads_dir))
            if (entry.is_regular_file())
                branches.push_back(entry.path().filename().string());

        std::sort(branches.begin(), branches.end());

        for (const auto &b : branches)
            std::cout << (b == cur ? "* " : "  ") << b << '\n';
        return;
    }

    // -----------------------------------------------------------------------
    // -d: delete branch.
    // -----------------------------------------------------------------------
    if (delete_branch)
    {
        const fs::path ref_path = heads_dir / name;
        if (!fs::exists(ref_path))
        {
            std::cerr << "error: branch '" << name << "' not found\n";
            std::exit(1);
        }
        if (name == current_branch(repo.git_dir()))
        {
            std::cerr << "error: cannot delete the currently checked-out branch\n";
            std::exit(1);
        }
        fs::remove(ref_path);
        std::cout << "Deleted branch " << name << '\n';
        return;
    }

    // -----------------------------------------------------------------------
    // Create branch pointing at HEAD.
    // -----------------------------------------------------------------------
    const fs::path ref_path = heads_dir / name;

    if (fs::exists(ref_path))
    {
        std::cerr << "fatal: a branch named '" << name << "' already exists\n";
        std::exit(1);
    }

    const std::string head_sha = resolve_head(repo.git_dir());
    if (head_sha.empty())
    {
        std::cerr << "fatal: not a valid object name: 'HEAD'\n";
        std::exit(1);
    }

    std::ofstream f(ref_path);
    if (!f)
        throw std::runtime_error("Could not create branch ref: " + ref_path.string());

    f << head_sha << '\n';
    std::cout << "Created branch '" << name << "' at " << head_sha.substr(0, 7) << '\n';
}
