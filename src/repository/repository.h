#pragma once

#include <filesystem>

class Repository
{
public:
    explicit Repository(const std::filesystem::path& root);

    // Walk parent directories from `start` upward until a .minigit directory
    // is found. Throws std::runtime_error if none is found.
    static Repository discover(const std::filesystem::path& start);

    const std::filesystem::path& root() const;
    const std::filesystem::path& git_dir() const;

    void init();

private:
    std::filesystem::path root_;
    std::filesystem::path git_dir_;
};