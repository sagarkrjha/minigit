#pragma once
#include <string>

// minigit pull [<remote> [<branch>]]
// Defaults: remote = "origin", branch = current HEAD branch.
void pull_command(const std::string &remote_name, const std::string &branch_name);
