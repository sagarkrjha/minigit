#pragma once

// Show differences between the working tree and the index (unstaged changes).
//   minigit diff [<path>...]
//
// Show differences between the index and the last commit (staged changes).
//   minigit diff --cached [<path>...]

#include <string>
#include <vector>

void diff_command(bool cached, const std::vector<std::string> &paths);
