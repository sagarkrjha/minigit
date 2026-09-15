#pragma once

#include <filesystem>

// Hash a file's content as a Blob object.
// If `write` is true, persist the blob to the object database.
void hash_object(const std::filesystem::path& path, bool write = false);