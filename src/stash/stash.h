#pragma once

#include <string>

// minigit stash [push]               → save working state, clean working tree
// minigit stash list                 → list all stash entries
// minigit stash pop [stash@{N}]      → apply and drop top (or Nth) stash
// minigit stash drop [stash@{N}]     → discard top (or Nth) stash entry
// minigit stash show [stash@{N}]     → show files in top (or Nth) stash
//
// Stash entries are commit objects stored in the object database.
// The stash stack is maintained in .minigit/stash (one SHA per line,
// most-recent first).  Each entry's commit message carries the WIP info
// needed for `stash list`.
void stash_command(const std::string& subcommand,
                   const std::string& stash_ref = "stash@{0}");
