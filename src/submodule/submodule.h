#pragma once

#include <string>
#include <vector>

// CLI entry point: minigit submodule [add|status|init|update|deinit|summary|foreach|sync] [<args>...]
int submodule_command(int argc, char const *argv[]);

// Individual subcommands
int submodule_add(int argc, char const *argv[]);
int submodule_status(int argc, char const *argv[]);
int submodule_init(int argc, char const *argv[]);
int submodule_update(int argc, char const *argv[]);
int submodule_deinit(int argc, char const *argv[]);
int submodule_summary(int argc, char const *argv[]);
int submodule_foreach(int argc, char const *argv[]);
int submodule_sync(int argc, char const *argv[]);
