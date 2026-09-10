#include "repository.h"
#include <fstream>
#include <stdexcept>

Repository::Repository(const std::filesystem::path &root) : root_(root), git_dir_(root / ".minigit")
{
}

Repository Repository::discover(const std::filesystem::path &start)
{
    auto current = std::filesystem::weakly_canonical(start);

    while (true)
    {
        const auto candidate = current / ".minigit";
        if (std::filesystem::exists(candidate) &&
            std::filesystem::is_directory(candidate))
        {
            return Repository(current);
        }

        const auto parent = current.parent_path();
        if (parent == current)
        {
            throw std::runtime_error(
                "fatal: not a minigit repository (or any of the parent "
                "directories): .minigit");
        }

        current = parent;
    }
}

const std::filesystem::path &Repository::root() const
{
    return root_;
}

const std::filesystem::path &Repository::git_dir() const
{
    return git_dir_;
}

void Repository::init()
{
    std::filesystem::create_directories(git_dir_ / "objects");
    std::filesystem::create_directories(git_dir_ / "refs" / "heads");
    std::filesystem::create_directories(git_dir_ / "refs" / "tags");

    if (!std::filesystem::exists(git_dir_ / "HEAD"))
    {
        std::ofstream head(git_dir_ / "HEAD");

        if (!head)
        {
            throw std::runtime_error("Could not create HEAD");
        }

        head << "ref: refs/heads/main\n";
    }
    if (!std::filesystem::exists(git_dir_ / "config"))
    {
        std::ofstream config(git_dir_ / "config");

        if (!config)
        {
            throw std::runtime_error("Could not create config");
        }
    }
    if (!std::filesystem::exists(git_dir_ / "index"))
    {
        std::ofstream index(git_dir_ / "index");

        if (!index)
        {
            throw std::runtime_error("Could not create index");
        }
    }
}