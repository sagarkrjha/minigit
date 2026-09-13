#include "remote.h"

#include "../remotes/config.h"
#include "../repository/repository.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

void remote_command(const std::string &sub,
                    const std::string &name,
                    const std::string &url)
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    RemoteConfig cfg(repo.git_dir() / "config");

    // -----------------------------------------------------------------------
    // remote (no args / -v)  →  list
    // -----------------------------------------------------------------------
    if (sub.empty() || sub == "-v" || sub == "list")
    {
        const auto &remotes = cfg.remotes();
        if (remotes.empty())
        {
            std::cout << "(no remotes configured)\n";
            return;
        }
        const bool verbose = (sub == "-v");
        for (const auto &r : remotes)
        {
            if (verbose)
                std::cout << r.name << "\t" << r.url << " (fetch)\n"
                          << r.name << "\t" << r.url << " (push)\n";
            else
                std::cout << r.name << '\n';
        }
        return;
    }

    // -----------------------------------------------------------------------
    // remote add <name> <url>
    // -----------------------------------------------------------------------
    if (sub == "add")
    {
        if (name.empty() || url.empty())
        {
            std::cerr << "usage: minigit remote add <name> <url>\n";
            std::exit(1);
        }
        try
        {
            cfg.add(name, url);
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
        std::cout << "Added remote '" << name << "' -> " << url << '\n';
        return;
    }

    // -----------------------------------------------------------------------
    // remote remove <name>
    // -----------------------------------------------------------------------
    if (sub == "remove" || sub == "rm")
    {
        if (name.empty())
        {
            std::cerr << "usage: minigit remote remove <name>\n";
            std::exit(1);
        }
        try
        {
            cfg.remove(name);
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
        std::cout << "Removed remote '" << name << "'\n";
        return;
    }

    std::cerr << "error: unknown remote subcommand '" << sub << "'\n";
    std::exit(1);
}
