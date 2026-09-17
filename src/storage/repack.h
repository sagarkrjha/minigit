#pragma once

#include "pack.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace minigit::storage {

struct RepackOptions
{
    bool pack_all{true};      // Pack all loose objects
    bool delete_loose{false};  // Remove redundant loose objects after packing (-d)
    int window{10};           // Delta search window size
};

struct RepackResult
{
    PackWriteResult pack_result;
    size_t loose_objects_found{0};
    size_t loose_objects_deleted{0};
    bool pack_created{false};
};

// Repacks objects in the repository containing repo_root.
RepackResult repack_repository(
    const std::filesystem::path& repo_root,
    const RepackOptions& options = {}
);

// Verifies integrity of a .pack or .idx file.
// If verbose is true, outputs detailed object listings.
bool verify_pack_file(
    const std::filesystem::path& path,
    bool verbose = false
);

// CLI command entry points
int repack_command(int argc, char const *argv[]);
int verify_pack_command(int argc, char const *argv[]);

} // namespace minigit::storage
