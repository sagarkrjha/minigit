#pragma once

#include <string>
#include <vector>

// Stage one or more files or directories (paths relative to current directory).
// Returns true if all paths staged successfully, false on error.
bool add_files(const std::vector<std::string> &paths);

