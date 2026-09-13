#pragma once

#include <string>

// minigit revert <commit>
//
// Creates a new commit that inverse-applies the changes introduced by
// <commit>, effectively undoing it without rewriting history.
//
// <commit> may be a full/short commit SHA, a branch name, or a tag name.
// An optional --author flag overrides the author identity for the revert commit.
//
// On a clean inverse-apply, creates a revert commit with message:
//   Revert "<original message>"
//
// If the inverse application conflicts with the current working tree,
// conflict markers are inserted, the index is left in a conflicted state,
// and the command exits with a non-zero status asking the user to resolve.
void revert_command(const std::string& target,
                    const std::string& author = "MiniGit User <user@minigit>");
