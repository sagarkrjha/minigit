#include "zlib_compress.h"

#include <stdexcept>
#include <string>
#include <zlib.h>

// ---------------------------------------------------------------------------
// Compress `data` with zlib deflate (Z_BEST_SPEED).
// Uses compressBound to allocate the worst-case output buffer in one shot,
// avoiding the need for a streaming loop on the compress path.
// ---------------------------------------------------------------------------
std::string zlib_compress(const std::string& data)
{
    uLongf bound = compressBound(static_cast<uLong>(data.size()));
    std::string out(bound, '\0');

    int rc = compress2(
        reinterpret_cast<Bytef*>(out.data()),
        &bound,
        reinterpret_cast<const Bytef*>(data.data()),
        static_cast<uLong>(data.size()),
        Z_BEST_SPEED);

    if (rc != Z_OK)
        throw std::runtime_error("zlib_compress: compress2 failed (code " +
                                 std::to_string(rc) + ")");

    out.resize(bound);   // trim to actual compressed size
    return out;
}

// ---------------------------------------------------------------------------
// Decompress zlib-deflated `compressed` data back to its original bytes.
// The original size is unknown, so we inflate in chunks and grow the output
// buffer dynamically until Z_STREAM_END is reached.
// ---------------------------------------------------------------------------
std::string zlib_decompress(const std::string& compressed)
{
    z_stream zs{};
    if (inflateInit(&zs) != Z_OK)
        throw std::runtime_error("zlib_decompress: inflateInit failed");

    zs.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
    zs.avail_in = static_cast<uInt>(compressed.size());

    std::string out;
    char buf[32768];   // 32 KiB chunk buffer

    int rc = Z_OK;
    do {
        zs.next_out  = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof(buf);

        rc = inflate(&zs, Z_NO_FLUSH);
        if (rc == Z_STREAM_ERROR || rc == Z_DATA_ERROR || rc == Z_MEM_ERROR)
        {
            inflateEnd(&zs);
            throw std::runtime_error(
                "zlib_decompress: inflate failed (code " +
                std::to_string(rc) + ")");
        }

        // Append whatever inflate wrote into buf this iteration.
        const std::size_t produced = sizeof(buf) - zs.avail_out;
        out.append(buf, produced);

    } while (rc != Z_STREAM_END);

    inflateEnd(&zs);
    return out;
}
