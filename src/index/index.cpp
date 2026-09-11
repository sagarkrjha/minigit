#include "index.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

Index::Index(const std::filesystem::path &index_path)
    : index_path_(index_path)
{
    std::ifstream file(index_path_);
    if (!file)
    {
        // A missing index file is fine — treat it as empty.
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        const auto sep = line.rfind(' ');
        if (sep == std::string::npos)
            throw std::runtime_error("Corrupt index entry: \"" + line + "\"");

        std::string path   = line.substr(0, sep);
        std::string blob_id = line.substr(sep + 1);
        entries_[std::move(path)] = std::move(blob_id);
    }
}

void Index::add(const std::filesystem::path &relative_path,
                const std::string &blob_id)
{
    entries_[relative_path.generic_string()] = blob_id;
}

void Index::write() const
{
    std::ofstream file(index_path_, std::ios::trunc);
    if (!file)
        throw std::runtime_error("Could not write index: " +
                                 index_path_.string());

    for (const auto &[path, blob_id] : entries_)
        file << path << ' ' << blob_id << '\n';
}

const std::unordered_map<std::string, std::string> &Index::entries() const
{
    return entries_;
}
