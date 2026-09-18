#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace minigit::remotes {

// Extracts raw packfile bytes from an HTTP response body,
// handling both raw packfile streams and multiplexed side-band streams.
std::string extract_pack_stream(const std::string &response_body);

// Unpacks all objects from a packfile binary stream into the given objects directory
// as loose CAS objects (".minigit/objects/XX/YY..."), properly resolving ref deltas.
// Returns the number of unpacked objects.
size_t unpack_pack_stream_to_db(
    const std::string &pack_data,
    const std::filesystem::path &objects_dir
);

// Unpacks all objects from a .pack file on disk into objects_dir.
size_t unpack_packfile_to_db(
    const std::filesystem::path &pack_path,
    const std::filesystem::path &objects_dir
);

} // namespace minigit::remotes
