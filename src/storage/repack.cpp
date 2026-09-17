#include "repack.h"

#include "object_database.h"
#include "core/sha256.h"
#include "core/zlib_compress.h"
#include "repository/repository.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace minigit::storage {

namespace {

bool is_hex_string(const std::string& str)
{
    return !str.empty() && std::all_of(str.begin(), str.end(), [](char c) {
        return std::isxdigit(static_cast<unsigned char>(c));
    });
}

std::string bytes_to_hex(const unsigned char* data, size_t len)
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

RepackResult repack_repository(
    const std::filesystem::path& repo_root,
    const RepackOptions& options)
{
    RepackResult result;

    std::filesystem::path objects_dir = repo_root / ".minigit" / "objects";
    if (!std::filesystem::exists(objects_dir) && std::filesystem::exists(repo_root / "objects"))
    {
        objects_dir = repo_root / "objects";
    }

    if (!std::filesystem::exists(objects_dir))
    {
        return result;
    }

    // 1. Scan for all loose objects in .minigit/objects/XX/YY...
    struct LooseItem
    {
        std::string id;
        std::filesystem::path path;
    };
    std::vector<LooseItem> loose_items;

    for (const auto& dir_entry : std::filesystem::directory_iterator(objects_dir))
    {
        if (!dir_entry.is_directory()) continue;
        const std::string dir_name = dir_entry.path().filename().string();
        if (dir_name.size() != 2 || !is_hex_string(dir_name)) continue;

        for (const auto& file_entry : std::filesystem::directory_iterator(dir_entry.path()))
        {
            if (!file_entry.is_regular_file()) continue;
            const std::string file_name = file_entry.path().filename().string();
            if (file_name.size() != 62 || !is_hex_string(file_name)) continue;

            loose_items.push_back({dir_name + file_name, file_entry.path()});
        }
    }

    result.loose_objects_found = loose_items.size();
    if (loose_items.empty())
    {
        return result;
    }

    // 2. Read and parse each loose object
    std::vector<PackEntry> entries;
    entries.reserve(loose_items.size());

    for (const auto& item : loose_items)
    {
        std::ifstream in(item.path, std::ios::binary);
        if (!in) continue;

        const std::string raw{
            std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>{}
        };

        std::string decompressed;
        try
        {
            decompressed = zlib_decompress(raw);
        }
        catch (const std::runtime_error&)
        {
            decompressed = raw;
        }

        const size_t space_pos = decompressed.find(' ');
        const size_t null_pos  = decompressed.find('\0');
        if (space_pos == std::string::npos || null_pos == std::string::npos || null_pos <= space_pos)
            continue;

        const std::string type_str = decompressed.substr(0, space_pos);
        const std::string payload  = decompressed.substr(null_pos + 1);

        PackEntry pe;
        pe.id = item.id;
        pe.type_name = type_str;
        if (type_str == "commit") pe.type_num = OBJ_COMMIT;
        else if (type_str == "tree") pe.type_num = OBJ_TREE;
        else if (type_str == "blob") pe.type_num = OBJ_BLOB;
        else if (type_str == "tag")  pe.type_num = OBJ_TAG;
        else pe.type_num = OBJ_BLOB;

        pe.payload = payload;
        pe.is_delta = false;
        entries.push_back(std::move(pe));
    }

    if (entries.empty())
    {
        return result;
    }

    // 3. Delta compression pass: sort by type, then size descending
    std::sort(entries.begin(), entries.end(), [](const PackEntry& a, const PackEntry& b) {
        if (a.type_num != b.type_num)
            return a.type_num < b.type_num;
        return a.payload.size() > b.payload.size();
    });

    const int window = std::max(1, options.window);
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].payload.size() < 32)
            continue;

        size_t start_idx = (i > static_cast<size_t>(window)) ? (i - window) : 0;
        size_t best_base_idx = i;
        std::string best_delta;

        for (size_t j = start_idx; j < i; ++j)
        {
            if (entries[j].type_num != entries[i].type_num)
                continue;
            if (entries[j].is_delta)
                continue; // Only delta against non-delta bases to keep depth = 1

            std::string d = create_delta(entries[j].payload, entries[i].payload);
            if (best_delta.empty() || d.size() < best_delta.size())
            {
                best_delta = std::move(d);
                best_base_idx = j;
            }
        }

        // Only adopt delta if it achieves at least 20% savings + 32-byte hash header
        if (!best_delta.empty() &&
            (best_delta.size() + 32) < static_cast<size_t>(entries[i].payload.size() * 0.8))
        {
            entries[i].is_delta = true;
            entries[i].base_id = entries[best_base_idx].id;
            entries[i].delta_data = std::move(best_delta);
        }
    }

    // 4. Write pack and idx files to .minigit/objects/pack/
    const std::filesystem::path pack_dir = objects_dir / "pack";
    result.pack_result = write_pack(pack_dir, entries);
    result.pack_created = true;

    // 5. Delete redundant loose objects if requested
    if (options.delete_loose)
    {
        for (const auto& item : loose_items)
        {
            std::error_code ec;
            if (std::filesystem::remove(item.path, ec))
            {
                result.loose_objects_deleted++;
                // Clean up directory if empty
                if (std::filesystem::is_empty(item.path.parent_path(), ec))
                {
                    std::filesystem::remove(item.path.parent_path(), ec);
                }
            }
        }
    }

    return result;
}

bool verify_pack_file(const std::filesystem::path& path, bool verbose)
{
    std::filesystem::path pack_path = path;
    std::filesystem::path idx_path  = path;

    if (path.extension() == ".pack")
    {
        idx_path.replace_extension(".idx");
    }
    else if (path.extension() == ".idx")
    {
        pack_path.replace_extension(".pack");
    }
    else
    {
        pack_path = path.string() + ".pack";
        idx_path  = path.string() + ".idx";
    }

    if (!std::filesystem::exists(pack_path))
    {
        std::cerr << "error: packfile not found: " << pack_path.string() << '\n';
        return false;
    }
    if (!std::filesystem::exists(idx_path))
    {
        std::cerr << "error: index file not found: " << idx_path.string() << '\n';
        return false;
    }

    auto idx = PackIndex::open(idx_path);
    if (!idx)
    {
        std::cerr << "error: invalid or corrupt index file: " << idx_path.string() << '\n';
        return false;
    }

    auto reader = PackReader::open(pack_path);
    if (!reader)
    {
        std::cerr << "error: invalid or corrupt packfile: " << pack_path.string() << '\n';
        return false;
    }

    // 1. Verify packfile SHA-256 trailer
    {
        std::ifstream pin(pack_path, std::ios::binary);
        if (!pin)
        {
            std::cerr << "error: cannot read packfile\n";
            return false;
        }
        pin.seekg(0, std::ios::end);
        size_t sz = pin.tellg();
        if (sz < 32)
        {
            std::cerr << "error: packfile too short\n";
            return false;
        }
        std::string content(sz - 32, '\0');
        pin.seekg(0, std::ios::beg);
        pin.read(content.data(), static_cast<std::streamsize>(content.size()));

        std::string computed_pack_checksum = sha256(content);
        if (computed_pack_checksum != reader->checksum())
        {
            std::cerr << "error: packfile checksum mismatch\n";
            return false;
        }

        // Verify index's recorded pack checksum
        if (idx->pack_checksum() != reader->checksum())
        {
            std::cerr << "error: index packfile checksum mismatch\n";
            return false;
        }
    }

    // 2. Verify index file SHA-256 self checksum
    {
        std::ifstream iin(idx_path, std::ios::binary);
        if (!iin)
        {
            std::cerr << "error: cannot read index file\n";
            return false;
        }
        iin.seekg(0, std::ios::end);
        size_t sz = iin.tellg();
        if (sz < 32)
        {
            std::cerr << "error: index file too short\n";
            return false;
        }
        std::string content(sz - 32, '\0');
        iin.seekg(0, std::ios::beg);
        iin.read(content.data(), static_cast<std::streamsize>(content.size()));

        std::string computed_idx_checksum = sha256(content);
        if (computed_idx_checksum != idx->idx_checksum())
        {
            std::cerr << "error: index checksum mismatch\n";
            return false;
        }
    }

    // 3. Verify each object CRC and record details
    const auto entries = idx->entries();
    size_t non_delta_count = 0;
    size_t delta_count = 0;

    for (const auto& entry : entries)
    {
        PackReader::ObjectInfo info;
        if (!reader->verify_object(entry.offset, entry.crc32, &info))
        {
            std::cerr << "error: object " << entry.sha << " failed verification\n";
            return false;
        }

        if (info.is_delta) delta_count++;
        else non_delta_count++;

        if (verbose)
        {
            std::string t_name;
            if (info.is_delta) t_name = "ref-delta";
            else if (info.type == OBJ_COMMIT) t_name = "commit";
            else if (info.type == OBJ_TREE)   t_name = "tree";
            else if (info.type == OBJ_BLOB)   t_name = "blob";
            else if (info.type == OBJ_TAG)    t_name = "tag";
            else t_name = "unknown";

            std::cout << entry.sha << " "
                      << std::left << std::setw(10) << t_name << " "
                      << std::right << std::setw(8) << info.unpacked_size << " "
                      << std::setw(8) << info.pack_size << " "
                      << std::setw(10) << entry.offset;

            if (info.is_delta)
            {
                std::cout << " 1 " << info.base_id;
            }
            std::cout << '\n';
        }
    }

    if (verbose)
    {
        std::cout << "non delta: " << non_delta_count << " objects\n";
        std::cout << "chain length = 1: " << delta_count << " objects\n";
        std::cout << pack_path.string() << ": OK\n";
    }
    else
    {
        std::cout << pack_path.filename().string() << ": OK (" << idx->count() << " objects)\n";
    }

    return true;
}

int repack_command(int argc, char const *argv[])
{
    RepackOptions opts;
    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-a")
        {
            opts.pack_all = true;
        }
        else if (arg == "-d")
        {
            opts.delete_loose = true;
        }
        else if (arg == "-w" && i + 1 < argc)
        {
            opts.window = std::stoi(argv[++i]);
        }
        else if (arg.rfind("--window=", 0) == 0)
        {
            opts.window = std::stoi(arg.substr(9));
        }
    }

    Repository repo(std::filesystem::current_path());
    try
    {
        repo = Repository::discover(std::filesystem::current_path());
    }
    catch (const std::runtime_error&)
    {
        std::cerr << "fatal: not a minigit repository (or any of the parent directories)\n";
        return 1;
    }

    RepackResult res = repack_repository(repo.root(), opts);
    if (!res.pack_created)
    {
        std::cout << "Nothing new to pack.\n";
        return 0;
    }

    std::cout << "Counting objects: " << res.loose_objects_found << ", done.\n";
    std::cout << "Compressing objects: 100% (" << res.loose_objects_found << "/" << res.loose_objects_found << "), done.\n";
    std::cout << "Writing objects: 100% (" << res.loose_objects_found << "/" << res.loose_objects_found << "), done.\n";
    std::cout << "Total " << res.loose_objects_found
              << " (delta " << res.pack_result.delta_objects << "), reused 0\n";

    if (opts.delete_loose && res.loose_objects_deleted > 0)
    {
        std::cout << "Removed " << res.loose_objects_deleted << " redundant loose objects.\n";
    }

    return 0;
}

int verify_pack_command(int argc, char const *argv[])
{
    bool verbose = false;
    std::vector<std::string> paths;

    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
        else
        {
            paths.push_back(arg);
        }
    }

    if (paths.empty())
    {
        std::cerr << "usage: minigit verify-pack [-v] <pack-file>...\n";
        return 1;
    }

    bool all_ok = true;
    for (const auto& p : paths)
    {
        if (!verify_pack_file(p, verbose))
        {
            all_ok = false;
        }
    }

    return all_ok ? 0 : 1;
}

} // namespace minigit::storage
