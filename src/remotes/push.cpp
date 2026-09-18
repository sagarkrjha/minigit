#include "push.h"
#include "smart_http.h"

#include "config.h"
#include "transfer.h"
#include "repository/repository.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

static std::string read_text(const fs::path &p)
{
    std::ifstream f(p);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string trim(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

// Derive current branch name from local HEAD.
static std::string current_branch(const fs::path &git_dir)
{
    const std::string raw = trim(read_text(git_dir / "HEAD"));
    if (raw.substr(0, 5) != "ref: ") return {}; // detached
    const std::string ref = raw.substr(5);
    const auto slash = ref.rfind('/');
    return (slash != std::string::npos) ? ref.substr(slash + 1) : ref;
}

// Resolve a branch name to its commit SHA.
static std::string resolve_branch(const fs::path &git_dir, const std::string &branch)
{
    return trim(read_text(git_dir / "refs" / "heads" / branch));
}

void push_command(const std::string &remote_name_in, const std::string &branch_name_in)
{
    Repository local = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    const std::string remote_name = remote_name_in.empty() ? "origin" : remote_name_in;

    // Resolve branch.
    std::string branch = branch_name_in;
    if (branch.empty())
    {
        branch = current_branch(local.git_dir());
        if (branch.empty())
        {
            std::cerr << "error: cannot push from a detached HEAD. "
                         "Specify a branch name.\n";
            std::exit(1);
        }
    }

    const std::string local_sha = resolve_branch(local.git_dir(), branch);
    if (local_sha.empty())
    {
        std::cerr << "error: branch '" << branch << "' does not exist locally\n";
        std::exit(1);
    }

    // Look up remote.
    RemoteConfig cfg(local.git_dir() / "config");
    const RemoteEntry *remote = cfg.find(remote_name);
    if (!remote)
    {
        std::cerr << "error: no such remote '" << remote_name << "'\n";
        std::exit(1);
    }

    if (minigit::remotes::is_http_url(remote->url))
    {
        minigit::remotes::push_http(local, remote_name, remote->url, branch);
        return;
    }

    const fs::path remote_root    = fs::weakly_canonical(remote->url);
    const fs::path remote_git     = remote_root / ".minigit";
    const fs::path remote_objects = remote_git / "objects";
    const fs::path local_objects  = local.git_dir() / "objects";

    if (!fs::exists(remote_git))
    {
        std::cerr << "fatal: remote URL is not a minigit repository: "
                  << remote->url << '\n';
        std::exit(1);
    }

    // Check fast-forward safety.
    const fs::path remote_branch_ref = remote_git / "refs" / "heads" / branch;
    const std::string remote_sha     = trim(read_text(remote_branch_ref));

    if (!remote_sha.empty())
    {
        // local_sha must be a descendant of remote_sha.
        if (remote_sha == local_sha)
        {
            std::cout << "Everything up-to-date.\n";
            return;
        }
        if (!transfer::is_ancestor(local_objects, remote_sha, local_sha))
        {
            std::cerr << "error: push rejected — non-fast-forward\n";
            std::cerr << "hint: fetch and merge remote changes before pushing.\n";
            std::exit(1);
        }
    }

    // Transfer objects from local to remote.
    const auto needed = transfer::missing_objects(
        local_objects, remote_objects, local_sha);
    transfer::transfer_objects(local_objects, remote_objects, needed);

    // Advance the remote branch ref.
    fs::create_directories(remote_branch_ref.parent_path());
    {
        std::ofstream f(remote_branch_ref, std::ios::trunc);
        if (!f)
            throw std::runtime_error("Cannot update remote ref: " +
                                     remote_branch_ref.string());
        f << local_sha << '\n';
    }

    // Update local remote-tracking ref.
    const fs::path tracking_ref =
        local.git_dir() / "refs" / "remotes" / remote_name / branch;
    fs::create_directories(tracking_ref.parent_path());
    {
        std::ofstream f(tracking_ref, std::ios::trunc);
        f << local_sha << '\n';
    }

    const std::string short_sha = local_sha.substr(0, 7);
    if (remote_sha.empty())
        std::cout << " * [new branch]      " << branch
                  << " -> " << remote_name << "/" << branch << '\n';
    else
        std::cout << "   " << remote_sha.substr(0, 7) << ".."
                  << short_sha << "  " << branch
                  << " -> " << remote_name << "/" << branch << '\n';

    std::cout << "Pushed " << needed.size() << " object(s).\n";
}
