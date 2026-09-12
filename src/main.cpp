#include <iostream>
#include <string>
#include <vector>

#include "commands/init.h"
#include "commands/hash_object.h"
#include "commands/cat_file.h"
#include "commands/add.h"
#include "commands/write_tree.h"
#include "commands/commit.h"
#include "commands/log.h"
#include "commands/status.h"
#include "commands/diff.h"
#include "commands/branch.h"
#include "commands/checkout.h"
#include "commands/switch_branch.h"

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: minigit <command>\n";
        return 1;
    }

    const std::string command = argv[1];

    if (command == "init")
    {
        init_repository();
    }
    else if (command == "add")
    {
        if (argc < 3)
        {
            std::cerr << "usage: minigit add <file>...\n";
            return 1;
        }

        std::vector<std::string> paths;
        for (int i = 2; i < argc; ++i)
            paths.emplace_back(argv[i]);

        add_files(paths);
    }
    else if (command == "hash-object")
    {
        bool write = false;
        int file_arg = 2;

        if (argc >= 3 && std::string(argv[2]) == "-w")
        {
            write = true;
            file_arg = 3;
        }

        if (argc <= file_arg)
        {
            std::cerr << "usage: minigit hash-object [-w] <file>\n";
            return 1;
        }

        hash_object(argv[file_arg], write);
    }
    else if (command == "cat-file")
    {
        // minigit cat-file (-t | -s | -p) <object>
        if (argc < 4)
        {
            std::cerr << "usage: minigit cat-file (-t | -s | -p) <object>\n";
            return 1;
        }
        cat_file(argv[2], argv[3]);
    }
    else if (command == "write-tree")
    {
        write_tree();
    }
    else if (command == "commit")
    {
        std::string message;
        std::string author = "MiniGit User <user@minigit>";

        for (int i = 2; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "-m" && i + 1 < argc)
                message = argv[++i];
            else if (arg == "--author" && i + 1 < argc)
                author = argv[++i];
        }

        if (message.empty())
        {
            std::cerr << "usage: minigit commit -m <message> [--author <author>]\n";
            return 1;
        }

        commit(message, author);
    }
    else if (command == "log")
    {
        log();
    }
    else if (command == "status")
    {
        status();
    }
    else if (command == "diff")
    {
        // minigit diff [--cached] [<path>...]
        bool cached = false;
        std::vector<std::string> paths;

        for (int i = 2; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--cached" || arg == "--staged")
                cached = true;
            else
                paths.emplace_back(arg);
        }

        diff_command(cached, paths);
    }
    else if (command == "branch")
    {
        // minigit branch              → list
        // minigit branch <name>       → create
        // minigit branch -d <name>    → delete
        bool del = false;
        std::string name;

        for (int i = 2; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "-d" && i + 1 < argc)
            {
                del = true;
                name = argv[++i];
            }
            else
            {
                name = arg;
            }
        }

        branch_command(name, del);
    }
    else if (command == "checkout")
    {
        if (argc < 3)
        {
            std::cerr << "usage: minigit checkout <branch-or-sha>\n";
            return 1;
        }
        checkout_command(argv[2]);
    }
    else if (command == "switch")
    {
        // minigit switch <branch>
        // minigit switch -c <new-branch>
        bool create = false;
        std::string branch;

        for (int i = 2; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "-c" && i + 1 < argc)
            {
                create = true;
                branch = argv[++i];
            }
            else
            {
                branch = arg;
            }
        }

        if (branch.empty())
        {
            std::cerr << "usage: minigit switch [-c] <branch>\n";
            return 1;
        }

        switch_command(branch, create);
    }
    else
    {
        std::cerr << "Unknown command: " << command << '\n';
        return 1;
    }
    return 0;
}