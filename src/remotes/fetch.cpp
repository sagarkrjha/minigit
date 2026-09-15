#include "fetch.h"

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

void fetch_command(const std::string &remote_name)
{
    Repository local = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    // Resolve remote URL.
    RemoteConfig cfg(local.git_dir() / "config");
    const RemoteEntry *remote = cfg.find(remote_name);
    if (!remote)
    {
        std::cerr << "error: no such remote '" << remote_name << "'\n";
        std::exit(1);
    }

    const fs::path remote_root   = fs::weakly_canonical(remote->url);
    const fs::path remote_git    = remote_root / ".minigit";
    const fs::path remote_heads  = remote_git / "refs" / "heads";
    const fs::path local_objects = local.git_dir() / "objects";
    const fs::path remote_objects= remote_git / "objects";

    if (!fs::exists(remote_git))
    {
        std::cerr << "fatal: remote '" << remote_name
                  << "' URL is not a minigit repository: " << remote->url << '\n';
        std::exit(1);
    }

    // Iterate every branch in the remote.
    if (!fs::exists(remote_heads))
    {
        std::cout << "Nothing to fetch (remote has no branches).\n";
        return;
    }

    std::size_t total_transferred = 0;

    for (const auto &entry : fs::directory_iterator(remote_heads))
    {
        if (!entry.is_regular_file()) continue;

        const std::string branch_name = entry.path().filename().string();
        const std::string remote_sha  = trim(read_text(entry.path()));
        if (remote_sha.empty()) continue;

        std::cout << "From " << remote->url << '\n';
        std::cout << " * [new branch]  " << branch_name
                  << " -> " << remote_name << "/" << branch_name << '\n';

        // Transfer missing objects.
        const auto needed = transfer::missing_objects(
            remote_objects, local_objects, remote_sha);
        transfer::transfer_objects(remote_objects, local_objects, needed);
        total_transferred += needed.size();

        // Update remote-tracking ref: refs/remotes/<remote>/<branch>
        const fs::path tracking_ref =
            local.git_dir() / "refs" / "remotes" / remote_name / branch_name;
        fs::create_directories(tracking_ref.parent_path());
        std::ofstream ref_file(tracking_ref, std::ios::trunc);
        if (!ref_file)
            throw std::runtime_error("Cannot update tracking ref: " +
                                     tracking_ref.string());
        ref_file << remote_sha << '\n';
    }

    std::cout << "Fetched " << total_transferred << " new object(s) from '"
              << remote_name << "'.\n";
}
