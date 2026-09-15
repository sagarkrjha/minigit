#pragma once

#include <string>

// minigit merge <branch>
//
// Performs a three-way merge of <branch> into the current branch.
// Requires a clean working tree (no unstaged modifications).
// On success, creates a merge commit with two parents.
// On conflict, writes conflict markers to the affected files and
// exits with a non-zero status, asking the user to resolve.
void merge_command(const std::string& branch,
                   const std::string& author = "MiniGit User <user@minigit>");
