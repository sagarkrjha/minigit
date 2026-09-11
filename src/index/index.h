#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

// Represents the staging area (.minigit/index).
// The on-disk format is one entry per line: "<path> <sha256>\n"
class Index
{
public:
    // Load the index from the given file path.
    explicit Index(const std::filesystem::path &index_path);

    // Stage a file: record path -> blob_id mapping.
    void add(const std::filesystem::path &relative_path,
             const std::string &blob_id);

    // Persist changes back to disk.
    void write() const;

    // Read-only access to the entries.
    const std::unordered_map<std::string, std::string> &entries() const;

private:
    std::filesystem::path index_path_;
    std::unordered_map<std::string, std::string> entries_; // path -> blob_id
};
