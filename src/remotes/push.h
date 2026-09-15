#pragma once
#include <string>

// minigit push [<remote> [<branch>]]
// Defaults: remote = "origin", branch = current HEAD branch.
void push_command(const std::string &remote_name, const std::string &branch_name);
