#pragma once
#include <string>

// minigit clone <src-path> [<dest-dir>]
void clone_command(const std::string &src_path, const std::string &dest_dir);
