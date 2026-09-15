#pragma once

#include <filesystem>
#include <string>

class ObjectDatabase
{
public:
    explicit ObjectDatabase(
        const std::filesystem::path& objects_directory
    );

    // Write object data under the given ID (noop if already present).
    void write(
        const std::string& id,
        const std::string& data
    );

    // Read raw object data for the given ID.
    // Throws std::runtime_error if the object is not found.
    std::string read(const std::string& id) const;

private:
    std::filesystem::path objects_directory_;
};