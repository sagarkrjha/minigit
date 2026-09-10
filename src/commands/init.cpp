#include "init.h"

#include "../repository/repository.h"

#include <iostream>
#include <exception>

void init_repository()
{
    Repository repo(std::filesystem::current_path());

    const bool already_exists =
        std::filesystem::exists(repo.git_dir());

    try
    {
        repo.init();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return;
    }

    if (already_exists)
    {
        std::cout
            << "Reinitialized existing mini_git repository in "
            << repo.git_dir() << "/\n";
    }
    else
    {
        std::cout
            << "Initialized empty mini_git repository in "
            << repo.git_dir() << "/\n";
    }
}