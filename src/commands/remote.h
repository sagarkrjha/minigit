#pragma once
#include <string>

// minigit remote add <name> <url>
// minigit remote remove <name>
// minigit remote -v              → list all remotes with their URLs
void remote_command(const std::string &sub, const std::string &name, const std::string &url);
