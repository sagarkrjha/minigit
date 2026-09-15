#pragma once
#include <string>

// minigit fetch [<remote>]
// Defaults to "origin" when no remote is specified.
void fetch_command(const std::string &remote_name);
