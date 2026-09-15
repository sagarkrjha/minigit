#pragma once

#include <string>

// Roll back HEAD (and optionally the index and working tree) to a target commit.
//
//   --soft   Move HEAD/branch pointer only. Index and working tree unchanged.
//   --mixed  Move HEAD + reset index to match the target commit's tree.
//            Working tree unchanged. (default when no mode is given)
//   --hard   Move HEAD + reset index + restore working tree files.
//
// `target` may be a commit SHA or a branch name.
void reset_command(const std::string& mode, const std::string& target);
