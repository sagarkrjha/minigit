#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace minigit::storage {
class PackIndex;
class PackReader;
}

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
    // Checks loose objects first, then falls back to .pack/.idx files.
    // Throws std::runtime_error if the object is not found.
    std::string read(const std::string& id) const;

    // Check whether an object exists (in loose storage or any packfile).
    bool contains(const std::string& id) const;

    // Invalidate and reload packfile readers (e.g. after repack).
    void reload_packs() const;

private:
    std::filesystem::path objects_directory_;

    struct PackHandle
    {
        std::shared_ptr<minigit::storage::PackIndex> index;
        std::shared_ptr<minigit::storage::PackReader> reader;
    };

    mutable std::vector<PackHandle> packs_;
    mutable bool packs_loaded_{false};

    void ensure_packs_loaded() const;
};
