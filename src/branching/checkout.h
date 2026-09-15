#pragma once

#include <string>

// Restore working tree and index from a commit (SHA or branch name).
// If given a branch name, HEAD is updated to track that branch.
// If given a raw SHA, HEAD is set to that SHA (detached HEAD mode).
//
//   minigit checkout <branch-or-sha>
void checkout_command(const std::string &target);
