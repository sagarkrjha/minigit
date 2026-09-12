#pragma once

#include <string>

// Switch to an existing branch (updates HEAD and working tree).
//   minigit switch <branch>
//
// Create and switch to a new branch.
//   minigit switch -c <branch>
void switch_command(const std::string &branch, bool create);
