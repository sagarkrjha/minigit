#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Forward declaration
class ObjectDatabase;

namespace minigit::storage {

// ---------------------------------------------------------------------------
// Pack object type codes (aligned with canonical Git specification)
// ---------------------------------------------------------------------------
constexpr int OBJ_COMMIT    = 1;
constexpr int OBJ_TREE      = 2;
constexpr int OBJ_BLOB      = 3;
constexpr int OBJ_TAG       = 4;
constexpr int OBJ_OFS_DELTA = 6;
constexpr int OBJ_REF_DELTA = 7;

// ---------------------------------------------------------------------------
// Delta compression API
// ---------------------------------------------------------------------------

// Computes a byte-level delta representing `target` relative to `base`.
std::string create_delta(const std::string& base, const std::string& target);

// Applies `delta` to `base`, reconstituting `target`.
// Throws std::runtime_error if delta cannot be applied or corrupted.
std::string apply_delta(const std::string& base, const std::string& delta);

// ---------------------------------------------------------------------------
// Pack creation structures
// ---------------------------------------------------------------------------

struct PackEntry
{
    std::string id;          // 64-character hex SHA-256
    std::string type_name;   // "commit", "tree", "blob", "tag"
    int type_num{0};         // OBJ_COMMIT, OBJ_TREE, OBJ_BLOB, OBJ_TAG
    std::string payload;     // Uncompressed raw body (without "<type> <size>\0" envelope)

    bool is_delta{false};
    std::string base_id;     // Base commit/tree/blob 64-char hex SHA-256
    std::string delta_data;  // Precomputed delta stream
};

struct PackWriteResult
{
    std::string pack_name;             // e.g. "pack-01234567..."
    std::filesystem::path pack_path;   // .minigit/objects/pack/pack-<hash>.pack
    std::filesystem::path idx_path;    // .minigit/objects/pack/pack-<hash>.idx
    size_t total_objects{0};
    size_t delta_objects{0};
    uint64_t pack_size{0};
    std::string checksum;              // 64-char hex SHA-256 pack checksum
};

// Writes `.pack` and `.idx` (Version 2) files into `pack_dir`.
PackWriteResult write_pack(
    const std::filesystem::path& pack_dir,
    const std::vector<PackEntry>& entries
);

// ---------------------------------------------------------------------------
// Pack Index (.idx Version 2) Reader
// ---------------------------------------------------------------------------

class PackIndex
{
public:
    struct Entry
    {
        std::string sha;      // 64-char hex SHA-256
        uint32_t crc32{0};    // Big-endian CRC32 of object data in packfile
        uint64_t offset{0};   // Byte offset in .pack file
    };

    static std::unique_ptr<PackIndex> open(const std::filesystem::path& idx_path);

    // Look up object offset by 64-char hex SHA. Returns -1 if not found.
    int64_t find_offset(const std::string& hex_sha) const;

    size_t count() const;
    std::vector<Entry> entries() const;

    std::string pack_checksum() const;  // Checksum of corresponding .pack file
    std::string idx_checksum() const;   // Checksum of this .idx file

    const std::filesystem::path& path() const { return path_; }

private:
    PackIndex(std::filesystem::path path, std::vector<uint8_t> data, uint32_t count);

    std::filesystem::path path_;
    std::vector<uint8_t> data_;
    uint32_t count_{0};
    std::vector<uint32_t> fanout_;
    const uint8_t* level2_names_{nullptr};
    const uint8_t* level3_crcs_{nullptr};
    const uint8_t* level4_offsets_{nullptr};
};

// ---------------------------------------------------------------------------
// Packfile (.pack Version 2) Reader
// ---------------------------------------------------------------------------

class PackReader
{
public:
    struct ObjectInfo
    {
        int type{0};                 // OBJ_COMMIT, OBJ_TREE, OBJ_BLOB, OBJ_TAG, OBJ_REF_DELTA
        uint64_t unpacked_size{0};
        uint64_t pack_size{0};       // Total bytes occupied in .pack (header + payload)
        std::string base_id;         // If is_delta == true
        bool is_delta{false};
    };

    static std::unique_ptr<PackReader> open(const std::filesystem::path& pack_path);

    // Read and fully resolve object at `offset` (including applying deltas if any).
    // Reconstitutes the standard loose envelope: "<type> <size>\0<payload>".
    std::string read_object(uint64_t offset, const ObjectDatabase& db) const;

    // Inspect object metadata at `offset` without full decompression.
    ObjectInfo inspect_object(uint64_t offset) const;

    // Verify object CRC32 at `offset` and optionally retrieve full object info.
    bool verify_object(uint64_t offset, uint32_t expected_crc32, ObjectInfo* out_info = nullptr) const;

    std::string checksum() const;
    const std::filesystem::path& path() const { return path_; }

private:
    explicit PackReader(std::filesystem::path path);

    std::filesystem::path path_;
};

} // namespace minigit::storage
