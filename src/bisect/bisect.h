#pragma once

#include <string>
#include <vector>

namespace minigit::bisect
{

// Entry point for porcelain command:
//   minigit bisect [start|bad|good|new|old|skip|reset|terms|log|replay|run] [<args>...]
int bisect_command(int argc, const char *argv[]);

} // namespace minigit::bisect
