#pragma once

#include <string>

// List all local branches (marks the current one with *).
//   minigit branch
//
// Create a new branch pointing at HEAD.
//   minigit branch <name>
//
// Delete a branch.
//   minigit branch -d <name>

void branch_command(const std::string &name = "", bool delete_branch = false);
