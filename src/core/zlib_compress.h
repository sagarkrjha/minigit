#pragma once

#include <string>

// ---------------------------------------------------------------------------
// Thin zlib wrapper used by ObjectDatabase for on-disk object compression.
//
// zlib_compress   – deflates `data` at Z_BEST_SPEED and returns the
//                   compressed byte string.  Throws std::runtime_error on
//                   failure.
//
// zlib_decompress – inflates a previously compressed byte string back to the
//                   original data.  Throws std::runtime_error on failure.
// ---------------------------------------------------------------------------

std::string zlib_compress(const std::string& data);
std::string zlib_decompress(const std::string& compressed);
