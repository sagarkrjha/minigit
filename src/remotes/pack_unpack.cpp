#include "pack_unpack.h"

#include "pkt_line.h"
#include "core/sha256.h"
#include "core/zlib_compress.h"
#include "storage/object_database.h"
#include "storage/pack.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <zlib.h>

namespace fs = std::filesystem;

namespace minigit::remotes {

namespace {

uint32_t read_be32(const uint8_t *ptr)
{
    return (static_cast<uint32_t>(ptr[0]) << 24) |
           (static_cast<uint32_t>(ptr[1]) << 16) |
           (static_cast<uint32_t>(ptr[2]) << 8)  |
           (static_cast<uint32_t>(ptr[3]));
}

int decode_pack_object_header(const char *&ptr, const char *end, uint64_t &size)
{
    if (ptr >= end) return 0;
    unsigned char c = static_cast<unsigned char>(*ptr++);
    int type = (c >> 4) & 7;
    size = c & 15;
    int shift = 4;
    while (c & 0x80)
    {
        if (ptr >= end) break;
        c = static_cast<unsigned char>(*ptr++);
        if (shift < 64)
        {
            size |= static_cast<uint64_t>(c & 0x7F) << shift;
        }
        shift += 7;
    }
    return type;
}

uint64_t decode_ofs_delta_offset(const char *&ptr, const char *end)
{
    if (ptr >= end) return 0;
    unsigned char c = static_cast<unsigned char>(*ptr++);
    uint64_t offset = c & 0x7F;
    while (c & 0x80)
    {
        if (ptr >= end) break;
        c = static_cast<unsigned char>(*ptr++);
        if (offset > (UINT64_MAX >> 7) - 1)
            break; // Overflow prevention
        offset = ((offset + 1) << 7) | (c & 0x7F);
    }
    return offset;
}

size_t skip_zlib_stream(const char *ptr, size_t max_len)
{
    z_stream strm{};
    if (inflateInit(&strm) != Z_OK) return 0;
    strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(ptr));
    strm.avail_in = static_cast<uInt>(max_len);
    char out_buf[8192];
    int ret = Z_OK;
    while (ret != Z_STREAM_END && strm.avail_in > 0)
    {
        strm.next_out = reinterpret_cast<Bytef *>(out_buf);
        strm.avail_out = sizeof(out_buf);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) break;
    }
    size_t consumed = strm.total_in;
    inflateEnd(&strm);
    return consumed;
}

std::string bytes_to_hex(const unsigned char *data, size_t len)
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

} // namespace

std::string extract_pack_stream(const std::string &response_body)
{
    // Check if whole response begins with 'PACK' directly
    if (response_body.rfind("PACK", 0) == 0)
        return response_body;

    // Try parsing as side-band multiplexed pkt-lines
    size_t offset = 0;
    std::string band1_data;
    bool found_sideband = false;

    while (offset < response_body.size())
    {
        PktLineResult pkt = read_pkt_line(response_body, offset);
        if (pkt.is_eof || pkt.is_flush)
            break;

        if (!pkt.payload.empty())
        {
            unsigned char channel = static_cast<unsigned char>(pkt.payload[0]);
            if (channel == 1)
            {
                // Band 1: Pack data
                found_sideband = true;
                band1_data.append(pkt.payload.substr(1));
            }
            else if (channel == 2)
            {
                // Band 2: Progress message (skip or log)
            }
            else if (channel == 3)
            {
                // Band 3: Error message
                throw std::runtime_error("Server error: " + pkt.payload.substr(1));
            }
        }
    }

    if (found_sideband && band1_data.rfind("PACK", 0) == 0)
    {
        return band1_data;
    }

    // Otherwise, search for 'PACK' magic signature in raw response (e.g. after NAK\n)
    size_t pack_pos = response_body.find("PACK");
    if (pack_pos != std::string::npos)
    {
        return response_body.substr(pack_pos);
    }

    throw std::runtime_error("No valid Git packfile found in server response");
}

size_t unpack_packfile_to_db(
    const fs::path &pack_path,
    const fs::path &objects_dir)
{
    std::ifstream in(pack_path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Could not open packfile: " + pack_path.string());

    in.seekg(0, std::ios::end);
    size_t file_sz = in.tellg();
    if (file_sz < 12 + 32)
        throw std::runtime_error("Packfile too small: " + pack_path.string());

    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> header(12);
    in.read(reinterpret_cast<char *>(header.data()), 12);

    if (header[0] != 'P' || header[1] != 'A' || header[2] != 'C' || header[3] != 'K')
        throw std::runtime_error("Invalid packfile header signature");

    uint32_t version = read_be32(header.data() + 4);
    if (version != 2)
        throw std::runtime_error("Unsupported packfile version: " + std::to_string(version));

    uint32_t total_objects = read_be32(header.data() + 8);
    if (total_objects == 0)
        return 0;

    // Load file into memory buffer for rapid scanning
    std::vector<char> pack_bytes(file_sz);
    in.seekg(0, std::ios::beg);
    in.read(pack_bytes.data(), file_sz);
    in.close();

    struct ObjectRecord
    {
        uint64_t offset{0};
        int type{0};
        uint64_t unpacked_size{0};
        bool is_delta{false};
        std::string base_sha;
        uint64_t base_offset{0};
    };

    std::vector<ObjectRecord> records;
    records.reserve(total_objects);

    uint64_t cur_offset = 12;
    for (uint32_t i = 0; i < total_objects; ++i)
    {
        if (cur_offset >= file_sz - 32) // last 32 bytes are checksum trailer
            throw std::runtime_error("Truncated packfile while parsing object table");

        const char *ptr = pack_bytes.data() + cur_offset;
        const char *end = pack_bytes.data() + file_sz - 32;
        const char *start = ptr;

        ObjectRecord rec;
        rec.offset = cur_offset;
        rec.type = decode_pack_object_header(ptr, end, rec.unpacked_size);

        if (rec.type == minigit::storage::OBJ_REF_DELTA)
        {
            rec.is_delta = true;
            if (ptr + 32 > end)
                throw std::runtime_error("Truncated OBJ_REF_DELTA base hash in packfile");
            rec.base_sha = bytes_to_hex(reinterpret_cast<const unsigned char *>(ptr), 32);
            ptr += 32;
        }
        else if (rec.type == minigit::storage::OBJ_OFS_DELTA)
        {
            rec.is_delta = true;
            uint64_t neg_offset = decode_ofs_delta_offset(ptr, end);
            rec.base_offset = cur_offset - neg_offset;
        }

        size_t consumed = skip_zlib_stream(ptr, end - ptr);
        if (consumed == 0)
            throw std::runtime_error("Failed to decompress object stream at offset " + std::to_string(cur_offset));

        size_t pack_size = (ptr - start) + consumed;
        cur_offset += pack_size;
        records.push_back(std::move(rec));
    }

    auto reader = minigit::storage::PackReader::open(pack_path);
    if (!reader)
        throw std::runtime_error("Could not instantiate PackReader for: " + pack_path.string());

    ObjectDatabase db(objects_dir);
    size_t unpacked_count = 0;

    auto write_envelope_to_db = [&](const std::string &envelope) -> std::string {
        std::string sha = sha256(envelope);
        fs::path obj_file = objects_dir / sha.substr(0, 2) / sha.substr(2);
        if (!fs::exists(obj_file))
        {
            fs::create_directories(obj_file.parent_path());
            std::string compressed = zlib_compress(envelope);
            std::ofstream out(obj_file, std::ios::binary | std::ios::trunc);
            if (!out)
                throw std::runtime_error("Could not write object file: " + obj_file.string());
            out.write(compressed.data(), static_cast<std::streamsize>(compressed.size()));
        }
        unpacked_count++;
        return sha;
    };

    // Pass 1: Write all base / non-delta objects
    std::vector<size_t> delta_indices;
    for (size_t i = 0; i < records.size(); ++i)
    {
        if (!records[i].is_delta)
        {
            std::string envelope = reader->read_object(records[i].offset, db);
            write_envelope_to_db(envelope);
        }
        else
        {
            delta_indices.push_back(i);
        }
    }

    // Pass 2: Resolve delta objects (handling potential chained deltas)
    while (!delta_indices.empty())
    {
        std::vector<size_t> remaining;
        bool progress = false;

        for (size_t idx : delta_indices)
        {
            try
            {
                std::string envelope = reader->read_object(records[idx].offset, db);
                write_envelope_to_db(envelope);
                progress = true;
            }
            catch (...)
            {
                // Base not yet resolved; retry on subsequent iteration
                remaining.push_back(idx);
            }
        }

        if (!progress && !remaining.empty())
        {
            throw std::runtime_error("Failed to resolve " + std::to_string(remaining.size()) + " delta object(s)");
        }
        delta_indices = std::move(remaining);
    }

    return unpacked_count;
}

size_t unpack_pack_stream_to_db(
    const std::string &pack_data,
    const fs::path &objects_dir)
{
    fs::create_directories(objects_dir);
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path temp_pack = objects_dir / ("temp_incoming_" + std::to_string(now) + ".pack");

    {
        std::ofstream out(temp_pack, std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("Could not write temporary packfile: " + temp_pack.string());
        out.write(pack_data.data(), static_cast<std::streamsize>(pack_data.size()));
    }

    size_t count = 0;
    try
    {
        count = unpack_packfile_to_db(temp_pack, objects_dir);
        std::error_code ec;
        fs::remove(temp_pack, ec);
    }
    catch (...)
    {
        std::error_code ec;
        fs::remove(temp_pack, ec);
        throw;
    }

    return count;
}

} // namespace minigit::remotes
