#include "remote.h"

#include "../remotes/config.h"
#include "../repository/repository.h"

#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <unordered_map>


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

    using RemoteHandler = std::function<void(RemoteConfig &, const std::string &, const std::string &, const std::string &)>;

    auto handle_list = [](RemoteConfig &c, const std::string &s, const std::string &, const std::string &) {
        const auto &remotes = c.remotes();
        if (remotes.empty())
        {
            std::cout << "(no remotes configured)\n";
            return;
        }
        const bool verbose = (s == "-v");
        for (const auto &r : remotes)
        {
            if (verbose)
                std::cout << r.name << "\t" << r.url << " (fetch)\n"
                          << r.name << "\t" << r.url << " (push)\n";
            else
                std::cout << r.name << '\n';
        }
    };

    auto handle_add = [](RemoteConfig &c, const std::string &, const std::string &rname, const std::string &rurl) {
        if (rname.empty() || rurl.empty())
        {
            std::cerr << "usage: minigit remote add <name> <url>\n";
            std::exit(1);
        }
        try
        {
            c.add(rname, rurl);
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
        std::cout << "Added remote '" << rname << "' -> " << rurl << '\n';
    };

    auto handle_remove = [](RemoteConfig &c, const std::string &, const std::string &rname, const std::string &) {
        if (rname.empty())
        {
            std::cerr << "usage: minigit remote remove <name>\n";
            std::exit(1);
        }
        try
        {
            c.remove(rname);
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
        std::cout << "Removed remote '" << rname << "'\n";
    };

    static const std::unordered_map<std::string, RemoteHandler> handlers = {
        {"",       handle_list},
        {"-v",     handle_list},
        {"list",   handle_list},
        {"add",    handle_add},
        {"remove", handle_remove},
        {"rm",     handle_remove}
    };

    const auto it = handlers.find(sub);
    if (it == handlers.end())
    {
        std::cerr << "error: unknown remote subcommand '" << sub << "'\n";
        std::exit(1);
    }

    it->second(cfg, sub, name, url);
}
