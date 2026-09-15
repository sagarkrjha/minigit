#pragma once

#include <string>

// Inspect a stored object by its SHA-256.
//
//   mode "-t"  → print the object type  (blob | tree | commit)
//   mode "-s"  → print the object size  (byte count of the body)
//   mode "-p"  → pretty-print the object contents
//
void cat_file(const std::string& mode, const std::string& object_id);
