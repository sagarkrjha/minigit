#include "pack.h"

#include "object_database.h"
#include "core/sha256.h"
#include "core/zlib_compress.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <zlib.h>

namespace minigit::storage {

// ---------------------------------------------------------------------------
// Byte conversion & serialization helpers
// ---------------------------------------------------------------------------

static std::string hex_to_bytes(const std::string& hex)
{
    std::string bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        char high = hex[i];
        char low = hex[i + 1];
        auto val = [](char c) -> unsigned char {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        bytes.push_back(static_cast<char>((val(high) << 4) | val(low)));
    }
    return bytes;
}

static std::string bytes_to_hex(const unsigned char* data, size_t len)
{
    static const char digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(len * 2);
    for (size_t i = 0; i < len; ++i)
    {
        hex.push_back(digits[(data[i] >> 4) & 0x0F]);
        hex.push_back(digits[data[i] & 0x0F]);
    }
    return hex;
}

static void write_be32(std::string& out, uint32_t val)
{
    out.push_back(static_cast<char>((val >> 24) & 0xFF));
    out.push_back(static_cast<char>((val >> 16) & 0xFF));
    out.push_back(static_cast<char>((val >> 8) & 0xFF));
    out.push_back(static_cast<char>(val & 0xFF));
}

static uint32_t read_be32(const uint8_t* ptr)
{
    return (static_cast<uint32_t>(ptr[0]) << 24) |
           (static_cast<uint32_t>(ptr[1]) << 16) |
           (static_cast<uint32_t>(ptr[2]) << 8)  |
           (static_cast<uint32_t>(ptr[3]));
}

static void encode_varint(std::string& out, uint64_t val)
{
    while (val >= 0x80)
    {
        out.push_back(static_cast<char>((val & 0x7F) | 0x80));
        val >>= 7;
    }
    out.push_back(static_cast<char>(val & 0x7F));
}

static uint64_t decode_varint(const char*& ptr, const char* end)
{
    uint64_t val = 0;
    int shift = 0;
    while (ptr < end)
    {
        unsigned char c = static_cast<unsigned char>(*ptr++);
        val |= static_cast<uint64_t>(c & 0x7F) << shift;
        if ((c & 0x80) == 0)
            break;
        shift += 7;
    }
    return val;
}

static void encode_pack_object_header(std::string& out, int type, uint64_t size)
{
    unsigned char byte = static_cast<unsigned char>((type << 4) | (size & 0x0F));
    size >>= 4;
    if (size > 0)
    {
        byte |= 0x80;
        out.push_back(static_cast<char>(byte));
        while (size > 0)
        {
            byte = static_cast<unsigned char>(size & 0x7F);
            size >>= 7;
            if (size > 0)
                byte |= 0x80;
            out.push_back(static_cast<char>(byte));
        }
    }
    else
    {
        out.push_back(static_cast<char>(byte));
    }
}

static int decode_pack_object_header(const char*& ptr, const char* end, uint64_t& size)
{
    if (ptr >= end) throw std::runtime_error("unexpected end of pack object");
    unsigned char c = static_cast<unsigned char>(*ptr++);
    int type = (c >> 4) & 0x07;
    size = c & 0x0F;
    int shift = 4;
    while (c & 0x80)
    {
        if (ptr >= end) throw std::runtime_error("truncated pack object header");
        c = static_cast<unsigned char>(*ptr++);
        size |= static_cast<uint64_t>(c & 0x7F) << shift;
        shift += 7;
    }
    return type;
}

static std::string decompress_pack_stream(const char* data, size_t max_len, size_t* bytes_consumed)
{
    z_stream zs{};
    if (inflateInit(&zs) != Z_OK)
        throw std::runtime_error("inflateInit failed");

    zs.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(data));
    zs.avail_in = static_cast<uInt>(max_len);

    std::string out;
    char buf[32768];

    int rc = Z_OK;
    do {
        zs.next_out  = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof(buf);

        rc = inflate(&zs, Z_NO_FLUSH);
        if (rc == Z_STREAM_ERROR || rc == Z_DATA_ERROR || rc == Z_MEM_ERROR)
        {
            inflateEnd(&zs);
            throw std::runtime_error("decompress_pack_stream: inflate failed (code " + std::to_string(rc) + ")");
        }

        out.append(buf, sizeof(buf) - zs.avail_out);
    } while (rc != Z_STREAM_END);

    if (bytes_consumed)
        *bytes_consumed = zs.total_in;

    inflateEnd(&zs);
    return out;
}

// ---------------------------------------------------------------------------
// Delta Compression Implementation
// ---------------------------------------------------------------------------

static uint32_t block_hash16(const char* p)
{
    uint32_t h = 0x811c9dc5;
    for (int i = 0; i < 16; ++i)
    {
        h ^= static_cast<unsigned char>(p[i]);
        h *= 0x01000193;
    }
    return h;
}

static void flush_insert(std::string& out, const std::string& target, size_t start, size_t len)
{
    while (len > 0)
    {
        size_t chunk = std::min<size_t>(len, 127);
        out.push_back(static_cast<char>(chunk & 0x7F));
        out.append(target.data() + start, chunk);
        start += chunk;
        len -= chunk;
    }
}

static void emit_copy_inst(std::string& out, uint32_t offset, uint32_t size)
{
    uint8_t op = 0x80;
    char off_bytes[4]; int num_off = 0;
    for (int b = 0; b < 4; ++b)
    {
        uint8_t byte = static_cast<uint8_t>((offset >> (b * 8)) & 0xFF);
        if (byte != 0)
        {
            op |= static_cast<uint8_t>(1 << b);
            off_bytes[num_off++] = static_cast<char>(byte);
        }
    }

    char sz_bytes[3]; int num_sz = 0;
    for (int b = 0; b < 3; ++b)
    {
        uint8_t byte = static_cast<uint8_t>((size >> (b * 8)) & 0xFF);
        if (byte != 0)
        {
            op |= static_cast<uint8_t>(1 << (b + 4));
            sz_bytes[num_sz++] = static_cast<char>(byte);
        }
    }

    out.push_back(static_cast<char>(op));
    out.append(off_bytes, num_off);
    out.append(sz_bytes, num_sz);
}

std::string create_delta(const std::string& base, const std::string& target)
{
    std::string out;
    encode_varint(out, base.size());
    encode_varint(out, target.size());

    if (base.size() < 16 || target.size() < 16)
    {
        flush_insert(out, target, 0, target.size());
        return out;
    }

    // Build hash index of base blocks
    std::unordered_map<uint32_t, std::vector<size_t>> base_index;
    for (size_t i = 0; i + 16 <= base.size(); i += 8)
    {
        uint32_t h = block_hash16(base.data() + i);
        base_index[h].push_back(i);
    }

    size_t t_pos = 0;
    size_t insert_start = 0;
    size_t insert_len = 0;

    while (t_pos < target.size())
    {
        size_t best_match_len = 0;
        size_t best_base_pos = 0;

        if (t_pos + 16 <= target.size())
        {
            uint32_t h = block_hash16(target.data() + t_pos);
            auto it = base_index.find(h);
            if (it != base_index.end())
            {
                for (size_t b_pos : it->second)
                {
                    size_t m = 0;
                    while (b_pos + m < base.size() && t_pos + m < target.size() &&
                           base[b_pos + m] == target[t_pos + m])
                    {
                        m++;
                    }
                    if (m > best_match_len)
                    {
                        best_match_len = m;
                        best_base_pos = b_pos;
                    }
                }
            }
        }

        if (best_match_len >= 16)
        {
            if (insert_len > 0)
            {
                flush_insert(out, target, insert_start, insert_len);
                insert_len = 0;
            }
            emit_copy_inst(out, static_cast<uint32_t>(best_base_pos), static_cast<uint32_t>(best_match_len));
            t_pos += best_match_len;
            insert_start = t_pos;
        }
        else
        {
            if (insert_len == 0)
                insert_start = t_pos;
            insert_len++;
            t_pos++;
        }
    }

    if (insert_len > 0)
    {
        flush_insert(out, target, insert_start, insert_len);
    }

    return out;
}

std::string apply_delta(const std::string& base, const std::string& delta)
{
    const char* ptr = delta.data();
    const char* end = delta.data() + delta.size();

    uint64_t src_sz = decode_varint(ptr, end);
    if (src_sz != base.size())
        throw std::runtime_error("delta base size mismatch: expected " + std::to_string(src_sz) + ", got " + std::to_string(base.size()));

    uint64_t tgt_sz = decode_varint(ptr, end);
    std::string out;
    out.reserve(tgt_sz);

    while (ptr < end)
    {
        unsigned char cmd = static_cast<unsigned char>(*ptr++);
        if (cmd & 0x80)
        {
            // Copy instruction
            uint32_t offset = 0;
            if (cmd & 0x01) { if (ptr >= end) throw std::runtime_error("truncated copy offset"); offset |= static_cast<unsigned char>(*ptr++); }
            if (cmd & 0x02) { if (ptr >= end) throw std::runtime_error("truncated copy offset"); offset |= static_cast<unsigned char>(*ptr++) << 8; }
            if (cmd & 0x04) { if (ptr >= end) throw std::runtime_error("truncated copy offset"); offset |= static_cast<unsigned char>(*ptr++) << 16; }
            if (cmd & 0x08) { if (ptr >= end) throw std::runtime_error("truncated copy offset"); offset |= static_cast<unsigned char>(*ptr++) << 24; }

            uint32_t size = 0;
            if (cmd & 0x10) { if (ptr >= end) throw std::runtime_error("truncated copy size"); size |= static_cast<unsigned char>(*ptr++); }
            if (cmd & 0x20) { if (ptr >= end) throw std::runtime_error("truncated copy size"); size |= static_cast<unsigned char>(*ptr++) << 8; }
            if (cmd & 0x40) { if (ptr >= end) throw std::runtime_error("truncated copy size"); size |= static_cast<unsigned char>(*ptr++) << 16; }
            if (size == 0) size = 0x10000;

            if (offset + size > base.size())
                throw std::runtime_error("delta copy out of bounds: offset=" + std::to_string(offset) + " size=" + std::to_string(size));

            out.append(base.data() + offset, size);
        }
        else if (cmd > 0)
        {
            // Insert instruction
            size_t len = cmd;
            if (ptr + len > end)
                throw std::runtime_error("truncated insert instruction");
            out.append(ptr, len);
            ptr += len;
        }
        else
        {
            throw std::runtime_error("invalid delta instruction opcode 0");
        }
    }

    if (out.size() != tgt_sz)
        throw std::runtime_error("delta target size mismatch: expected " + std::to_string(tgt_sz) + ", got " + std::to_string(out.size()));

    return out;
}

// ---------------------------------------------------------------------------
// Pack Writer Implementation
// ---------------------------------------------------------------------------

PackWriteResult write_pack(
    const std::filesystem::path& pack_dir,
    const std::vector<PackEntry>& entries)
{
    std::filesystem::create_directories(pack_dir);

    // 1. Prepare pack file buffer
    std::string pack_data;
    // 12-byte header: 'PACK', version 2, number of objects
    pack_data.append("PACK");
    write_be32(pack_data, 2);
    write_be32(pack_data, static_cast<uint32_t>(entries.size()));

    struct ObjectRecord
    {
        std::string sha_hex;
        std::string raw_sha32;
        uint32_t crc32{0};
        uint64_t offset{0};
    };

    std::vector<ObjectRecord> records;
    records.reserve(entries.size());

    size_t delta_count = 0;

    // 2. Serialize objects into packfile
    for (const auto& e : entries)
    {
        ObjectRecord rec;
        rec.sha_hex = e.id;
        rec.raw_sha32 = hex_to_bytes(e.id);
        rec.offset = pack_data.size();

        std::string obj_bytes;
        if (e.is_delta)
        {
            delta_count++;
            encode_pack_object_header(obj_bytes, OBJ_REF_DELTA, e.delta_data.size());
            obj_bytes.append(hex_to_bytes(e.base_id));
            obj_bytes.append(zlib_compress(e.delta_data));
        }
        else
        {
            int type_code = e.type_num;
            if (type_code == 0)
            {
                if (e.type_name == "commit") type_code = OBJ_COMMIT;
                else if (e.type_name == "tree") type_code = OBJ_TREE;
                else if (e.type_name == "blob") type_code = OBJ_BLOB;
                else if (e.type_name == "tag") type_code = OBJ_TAG;
                else type_code = OBJ_BLOB;
            }
            encode_pack_object_header(obj_bytes, type_code, e.payload.size());
            obj_bytes.append(zlib_compress(e.payload));
        }

        // Compute CRC32 over the object's bytes in the packfile
        uLong crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(obj_bytes.data()), static_cast<uInt>(obj_bytes.size()));
        rec.crc32 = static_cast<uint32_t>(crc);

        pack_data.append(obj_bytes);
        records.push_back(std::move(rec));
    }

    // 3. Compute packfile SHA-256 trailer
    const std::string pack_checksum_hex = sha256(pack_data);
    const std::string pack_checksum_raw = hex_to_bytes(pack_checksum_hex);
    pack_data.append(pack_checksum_raw);

    // 4. Sort records lexicographically by 32-byte binary SHA-256 for the .idx table
    std::sort(records.begin(), records.end(), [](const ObjectRecord& a, const ObjectRecord& b) {
        return a.raw_sha32 < b.raw_sha32;
    });

    // 5. Build .idx (Version 2) buffer
    std::string idx_data;
    // Header: \xFF t O c, version 2
    idx_data.push_back(static_cast<char>(0xFF));
    idx_data.append("tOc");
    write_be32(idx_data, 2);

    // Level 1: 256-entry fan-out table
    uint32_t fanout[256] = {0};
    for (const auto& r : records)
    {
        uint8_t first_byte = static_cast<uint8_t>(r.raw_sha32[0]);
        fanout[first_byte]++;
    }
    // Cumulative sum
    for (int i = 1; i < 256; ++i)
        fanout[i] += fanout[i - 1];

    for (int i = 0; i < 256; ++i)
        write_be32(idx_data, fanout[i]);

    // Level 2: 32-byte object SHA hashes
    for (const auto& r : records)
        idx_data.append(r.raw_sha32);

    // Level 3: 4-byte CRC32s
    for (const auto& r : records)
        write_be32(idx_data, r.crc32);

    // Level 4: 4-byte offsets
    for (const auto& r : records)
        write_be32(idx_data, static_cast<uint32_t>(r.offset));

    // Trailer: 32-byte pack checksum, then 32-byte idx checksum
    idx_data.append(pack_checksum_raw);

    const std::string idx_checksum_hex = sha256(idx_data);
    const std::string idx_checksum_raw = hex_to_bytes(idx_checksum_hex);
    idx_data.append(idx_checksum_raw);

    // 6. Write files to disk
    const std::string base_name = "pack-" + pack_checksum_hex;
    const auto pack_path = pack_dir / (base_name + ".pack");
    const auto idx_path  = pack_dir / (base_name + ".idx");

    {
        std::ofstream p_out(pack_path, std::ios::binary | std::ios::trunc);
        if (!p_out) throw std::runtime_error("could not write packfile: " + pack_path.string());
        p_out.write(pack_data.data(), static_cast<std::streamsize>(pack_data.size()));
    }

    {
        std::ofstream i_out(idx_path, std::ios::binary | std::ios::trunc);
        if (!i_out) throw std::runtime_error("could not write pack index: " + idx_path.string());
        i_out.write(idx_data.data(), static_cast<std::streamsize>(idx_data.size()));
    }

    PackWriteResult res;
    res.pack_name = base_name;
    res.pack_path = pack_path;
    res.idx_path = idx_path;
    res.total_objects = entries.size();
    res.delta_objects = delta_count;
    res.pack_size = pack_data.size();
    res.checksum = pack_checksum_hex;
    return res;
}

// ---------------------------------------------------------------------------
// PackIndex Implementation
// ---------------------------------------------------------------------------

PackIndex::PackIndex(std::filesystem::path path, std::vector<uint8_t> data, uint32_t count)
    : path_(std::move(path)), data_(std::move(data)), count_(count)
{
    fanout_.resize(256);
    const uint8_t* p = data_.data() + 8; // skip magic and version
    for (int i = 0; i < 256; ++i)
    {
        fanout_[i] = read_be32(p + i * 4);
    }
    level2_names_   = data_.data() + 8 + 256 * 4;
    level3_crcs_    = level2_names_ + count_ * 32;
    level4_offsets_ = level3_crcs_  + count_ * 4;
}

std::unique_ptr<PackIndex> PackIndex::open(const std::filesystem::path& idx_path)
{
    std::ifstream in(idx_path, std::ios::binary);
    if (!in) return nullptr;

    in.seekg(0, std::ios::end);
    size_t sz = in.tellg();
    in.seekg(0, std::ios::beg);

    if (sz < 8 + 256 * 4 + 32 + 32)
        return nullptr;

    std::vector<uint8_t> data(sz);
    in.read(reinterpret_cast<char*>(data.data()), sz);

    // Verify magic (\xFF t O c) and version 2
    if (data[0] != 0xFF || data[1] != 't' || data[2] != 'O' || data[3] != 'c')
        return nullptr;
    if (read_be32(data.data() + 4) != 2)
        return nullptr;

    uint32_t total_objects = read_be32(data.data() + 8 + 255 * 4);
    size_t expected_size = 8 + 256 * 4 + total_objects * 32 + total_objects * 4 + total_objects * 4 + 32 + 32;
    if (sz < expected_size)
        return nullptr;

    return std::unique_ptr<PackIndex>(new PackIndex(idx_path, std::move(data), total_objects));
}

int64_t PackIndex::find_offset(const std::string& hex_sha) const
{
    if (hex_sha.size() != 64) return -1;
    const std::string raw_sha = hex_to_bytes(hex_sha);
    const uint8_t first_byte = static_cast<uint8_t>(raw_sha[0]);

    uint32_t low = (first_byte == 0) ? 0 : fanout_[first_byte - 1];
    uint32_t high = fanout_[first_byte];

    while (low < high)
    {
        uint32_t mid = low + (high - low) / 2;
        int cmp = std::memcmp(raw_sha.data(), level2_names_ + mid * 32, 32);
        if (cmp == 0)
        {
            return read_be32(level4_offsets_ + mid * 4);
        }
        else if (cmp < 0)
        {
            high = mid;
        }
        else
        {
            low = mid + 1;
        }
    }

    return -1;
}

size_t PackIndex::count() const
{
    return count_;
}

std::vector<PackIndex::Entry> PackIndex::entries() const
{
    std::vector<Entry> result;
    result.reserve(count_);
    for (uint32_t i = 0; i < count_; ++i)
    {
        Entry e;
        e.sha = bytes_to_hex(level2_names_ + i * 32, 32);
        e.crc32 = read_be32(level3_crcs_ + i * 4);
        e.offset = read_be32(level4_offsets_ + i * 4);
        result.push_back(std::move(e));
    }
    return result;
}

std::string PackIndex::pack_checksum() const
{
    // The 32 bytes preceding the last 32 bytes of the .idx file
    const uint8_t* ptr = data_.data() + data_.size() - 64;
    return bytes_to_hex(ptr, 32);
}

std::string PackIndex::idx_checksum() const
{
    // The last 32 bytes of the .idx file
    const uint8_t* ptr = data_.data() + data_.size() - 32;
    return bytes_to_hex(ptr, 32);
}

// ---------------------------------------------------------------------------
// PackReader Implementation
// ---------------------------------------------------------------------------

PackReader::PackReader(std::filesystem::path path)
    : path_(std::move(path))
{
}

std::unique_ptr<PackReader> PackReader::open(const std::filesystem::path& pack_path)
{
    if (!std::filesystem::exists(pack_path)) return nullptr;
    return std::unique_ptr<PackReader>(new PackReader(pack_path));
}

std::string PackReader::checksum() const
{
    std::ifstream in(path_, std::ios::binary);
    if (!in) return {};
    in.seekg(0, std::ios::end);
    size_t sz = in.tellg();
    if (sz < 32) return {};
    in.seekg(sz - 32, std::ios::beg);
    unsigned char buf[32];
    in.read(reinterpret_cast<char*>(buf), 32);
    return bytes_to_hex(buf, 32);
}

PackReader::ObjectInfo PackReader::inspect_object(uint64_t offset) const
{
    std::ifstream in(path_, std::ios::binary);
    if (!in) throw std::runtime_error("could not open packfile: " + path_.string());

    in.seekg(0, std::ios::end);
    size_t sz = in.tellg();
    if (offset >= sz) throw std::runtime_error("packfile offset out of bounds");

    in.seekg(offset, std::ios::beg);
    std::vector<char> buffer(std::min<size_t>(sz - offset, 65536));
    in.read(buffer.data(), buffer.size());

    const char* ptr = buffer.data();
    const char* end = buffer.data() + in.gcount();

    ObjectInfo info;
    info.type = decode_pack_object_header(ptr, end, info.unpacked_size);
    info.is_delta = (info.type == OBJ_REF_DELTA);

    if (info.is_delta)
    {
        if (ptr + 32 > end) throw std::runtime_error("truncated delta base sha in packfile");
        info.base_id = bytes_to_hex(reinterpret_cast<const unsigned char*>(ptr), 32);
    }

    return info;
}

bool PackReader::verify_object(uint64_t offset, uint32_t expected_crc32, ObjectInfo* out_info) const
{
    std::ifstream in(path_, std::ios::binary);
    if (!in) return false;

    in.seekg(0, std::ios::end);
    size_t file_sz = in.tellg();
    if (offset >= file_sz) return false;

    size_t remaining = file_sz - offset;
    std::vector<char> buf(remaining);
    in.seekg(offset, std::ios::beg);
    in.read(buf.data(), remaining);

    const char* ptr = buf.data();
    const char* end = buf.data() + in.gcount();

    uint64_t unpacked_sz = 0;
    int type = decode_pack_object_header(ptr, end, unpacked_sz);

    std::string base_id;
    if (type == OBJ_REF_DELTA)
    {
        if (ptr + 32 > end) return false;
        base_id = bytes_to_hex(reinterpret_cast<const unsigned char*>(ptr), 32);
        ptr += 32;
    }

    size_t consumed = 0;
    std::string inflated;
    try
    {
        inflated = decompress_pack_stream(ptr, end - ptr, &consumed);
    }
    catch (...)
    {
        return false;
    }

    size_t total_pack_bytes = (ptr - buf.data()) + consumed;
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(buf.data()), static_cast<uInt>(total_pack_bytes));

    if (static_cast<uint32_t>(crc) != expected_crc32)
        return false;

    if (out_info)
    {
        out_info->type = type;
        out_info->pack_size = total_pack_bytes;
        out_info->is_delta = (type == OBJ_REF_DELTA);
        out_info->base_id = base_id;
        if (out_info->is_delta)
        {
            const char* d_ptr = inflated.data();
            const char* d_end = inflated.data() + inflated.size();
            decode_varint(d_ptr, d_end); // skip base size
            out_info->unpacked_size = decode_varint(d_ptr, d_end);
        }
        else
        {
            out_info->unpacked_size = unpacked_sz;
        }
    }

    return true;
}

std::string PackReader::read_object(uint64_t offset, const ObjectDatabase& db) const
{
    std::ifstream in(path_, std::ios::binary);
    if (!in) throw std::runtime_error("could not open packfile: " + path_.string());

    in.seekg(0, std::ios::end);
    size_t file_sz = in.tellg();
    if (offset >= file_sz) throw std::runtime_error("pack offset out of bounds");

    // Read remaining bytes from offset
    size_t remaining_bytes = file_sz - offset;
    std::vector<char> buf(remaining_bytes);
    in.seekg(offset, std::ios::beg);
    in.read(buf.data(), remaining_bytes);

    const char* ptr = buf.data();
    const char* end = buf.data() + in.gcount();

    uint64_t unpacked_size = 0;
    int type = decode_pack_object_header(ptr, end, unpacked_size);

    if (type == OBJ_COMMIT || type == OBJ_TREE || type == OBJ_BLOB || type == OBJ_TAG)
    {
        size_t consumed = 0;
        std::string payload = decompress_pack_stream(ptr, end - ptr, &consumed);

        std::string type_name;
        if (type == OBJ_COMMIT) type_name = "commit";
        else if (type == OBJ_TREE) type_name = "tree";
        else if (type == OBJ_BLOB) type_name = "blob";
        else if (type == OBJ_TAG) type_name = "tag";

        return type_name + " " + std::to_string(unpacked_size) + '\0' + payload;
    }
    else if (type == OBJ_REF_DELTA)
    {
        if (ptr + 32 > end) throw std::runtime_error("truncated OBJ_REF_DELTA base hash");
        std::string base_sha = bytes_to_hex(reinterpret_cast<const unsigned char*>(ptr), 32);
        ptr += 32;

        size_t consumed = 0;
        std::string delta_data = decompress_pack_stream(ptr, end - ptr, &consumed);

        // Fetch base object from ObjectDatabase (can resolve from loose or other packfiles)
        std::string base_obj = db.read(base_sha);
        size_t null_pos = base_obj.find('\0');
        size_t space_pos = base_obj.find(' ');
        if (null_pos == std::string::npos || space_pos == std::string::npos)
            throw std::runtime_error("corrupt base object: " + base_sha);

        std::string type_name = base_obj.substr(0, space_pos);
        std::string base_payload = base_obj.substr(null_pos + 1);

        std::string target_payload = apply_delta(base_payload, delta_data);
        return type_name + " " + std::to_string(target_payload.size()) + '\0' + target_payload;
    }
    else
    {
        throw std::runtime_error("unsupported pack object type: " + std::to_string(type));
    }
}

} // namespace minigit::storage
