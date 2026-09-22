#include "object_database.h"

#include "pack.h"
#include "core/logger.h"
#include "core/zlib_compress.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

ObjectDatabase::ObjectDatabase(
    const std::filesystem::path &objects_directory)
    : objects_directory_(objects_directory)
{
    if (!std::filesystem::exists(objects_directory_))
    {
        const auto commondir_file = objects_directory_.parent_path() / "commondir";
        if (std::filesystem::exists(commondir_file))
        {
            std::ifstream f(commondir_file);
            std::string rel;
            if (std::getline(f, rel))
            {
                while (!rel.empty() && (rel.back() == '\r' || rel.back() == '\n' || rel.back() == ' '))
                    rel.pop_back();
                size_t cstart = 0;
                while (cstart < rel.size() && rel[cstart] == ' ')
                    cstart++;
                rel = rel.substr(cstart);

                std::filesystem::path cpath(rel);
                auto common = cpath.is_relative()
                    ? std::filesystem::weakly_canonical(objects_directory_.parent_path() / cpath)
                    : std::filesystem::weakly_canonical(cpath);
                if (std::filesystem::exists(common / "objects"))
                {
                    objects_directory_ = common / "objects";
                }
            }
        }
    }
}

void ObjectDatabase::write(
    const std::string &id,
    const std::string &data)
{
    LOG_TRACE("storage", "writing object " << id << " (" << data.size() << " bytes)");
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

    // Compress the object envelope before persisting to disk.
    const std::string compressed = zlib_compress(data);

    std::ofstream out(file_path, std::ios::binary);
    if (!out)
    {
        throw std::runtime_error(
            "Could not write object: " + file_path.string());
    }

    out.write(compressed.data(), static_cast<std::streamsize>(compressed.size()));
}

void ObjectDatabase::ensure_packs_loaded() const
{
    if (packs_loaded_)
        return;

    packs_loaded_ = true;
    packs_.clear();

    const auto pack_dir = objects_directory_ / "pack";
    if (!std::filesystem::exists(pack_dir) || !std::filesystem::is_directory(pack_dir))
        return;

    for (const auto& entry : std::filesystem::directory_iterator(pack_dir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() == ".idx")
        {
            auto pack_path = entry.path();
            pack_path.replace_extension(".pack");
            if (std::filesystem::exists(pack_path))
            {
                auto idx = minigit::storage::PackIndex::open(entry.path());
                auto reader = minigit::storage::PackReader::open(pack_path);
                if (idx && reader)
                {
                    packs_.push_back({std::move(idx), std::move(reader)});
                }
            }
        }
    }
}

void ObjectDatabase::reload_packs() const
{
    packs_loaded_ = false;
    packs_.clear();
    ensure_packs_loaded();
}

bool ObjectDatabase::contains(const std::string &id) const
{
    if (id.size() >= 2)
    {
        const std::string prefix    = id.substr(0, 2);
        const std::string remainder = id.substr(2);
        const auto file_path = objects_directory_ / prefix / remainder;
        if (std::filesystem::exists(file_path))
        {
            return true;
        }
    }

    ensure_packs_loaded();
    for (const auto& pack : packs_)
    {
        if (pack.index->find_offset(id) >= 0)
        {
            return true;
        }
    }

    return false;
}

std::string ObjectDatabase::read(const std::string &id) const
{
    LOG_TRACE("storage", "reading object " << id);
    if (id.size() >= 2)
    {
        const std::string prefix    = id.substr(0, 2);
        const std::string remainder = id.substr(2);
        const auto file_path = objects_directory_ / prefix / remainder;

        std::ifstream in(file_path, std::ios::binary);
        if (in)
        {
            const std::string raw{
                std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>{}};

            // Decompress the stored data.  If decompression fails the object was
            // written by an older version of MiniGit (uncompressed), so fall back
            // to returning the raw bytes directly — ensuring backward compatibility
            // with repositories created before v0.5.0.
            try
            {
                return zlib_decompress(raw);
            }
            catch (const std::runtime_error&)
            {
                return raw;
            }
        }
    }

    // Fall back to packfile search
    ensure_packs_loaded();
    for (const auto& pack : packs_)
    {
        const int64_t offset = pack.index->find_offset(id);
        if (offset >= 0)
        {
            return pack.reader->read_object(static_cast<uint64_t>(offset), *this);
        }
    }

    throw std::runtime_error("object not found: " + id);
}
