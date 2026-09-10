#include "object_database.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

ObjectDatabase::ObjectDatabase(
    const std::filesystem::path &objects_directory)
    : objects_directory_(objects_directory)
{
}

void ObjectDatabase::write(
    const std::string &id,
    const std::string &data)
{
    const std::string prefix = id.substr(0, 2);
    const std::string remainder = id.substr(2);

    const auto directory = objects_directory_ / prefix;
    const auto file_path = directory / remainder;

    // Object already stored — no need to rewrite.
    if (std::filesystem::exists(file_path))
    {
        return;
    }

    std::filesystem::create_directories(directory);

    std::ofstream out(file_path, std::ios::binary);
    if (!out)
    {
        throw std::runtime_error(
            "Could not write object: " + file_path.string());
    }

    out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

std::string ObjectDatabase::read(const std::string &id) const
{
    const std::string prefix    = id.substr(0, 2);
    const std::string remainder = id.substr(2);
    const auto file_path = objects_directory_ / prefix / remainder;

    std::ifstream in(file_path, std::ios::binary);
    if (!in)
        throw std::runtime_error("object not found: " + id);

    return std::string(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
}