#include "dispatcher.h"

#include "repository/init.h"
#include "storage/hash_object.h"
#include "storage/cat_file.h"
#include "staging/add.h"
#include "staging/write_tree.h"
#include "history/commit.h"
#include "history/log.h"
#include "history/show.h"
#include "staging/status.h"
#include "staging/clean.h"
#include "staging/ls_files.h"
#include "storage/ls_tree.h"
#include "diff/diff.h"
#include "branching/branch.h"
#include "branching/checkout.h"
#include "branching/switch_branch.h"
#include "staging/reset.h"
#include "branching/tag.h"
#include "merge/merge.h"
#include "merge/revert.h"
#include "merge/cherry_pick.h"
#include "merge/rebase.h"
#include "stash/stash.h"
#include "remotes/remote.h"
#include "remotes/clone.h"
#include "remotes/fetch.h"
#include "remotes/push.h"
#include "remotes/pull.h"
#include "storage/repack.h"
#include "worktree/worktree.h"
#include "submodule/submodule.h"
#include "bisect/bisect.h"
#include "core/logger.h"
#include "core/version.h"
#include "install/install.h"
#include "update/update.h"
#include "update/notifier.h"

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using CommandHandler = std::function<int(int argc, char const *argv[])>;

namespace {

int cmd_init(int /*argc*/, char const * /*argv*/[])
{
    init_repository();
    return 0;
}

int cmd_add(int argc, char const *argv[])
{
    if (argc < 3)
    {
        std::cerr << "usage: minigit add <file>...\n";
        return 1;
    }

    std::vector<std::string> paths;
    for (int i = 2; i < argc; ++i)
        paths.emplace_back(argv[i]);

    return add_files(paths) ? 0 : 1;
}

int cmd_hash_object(int argc, char const *argv[])
{
    bool write = false;
    int file_arg = 2;

    if (argc >= 3 && std::string(argv[2]) == "-w")
    {
        write = true;
        file_arg = 3;
    }

    if (argc <= file_arg)
    {
        std::cerr << "usage: minigit hash-object [-w] <file>\n";
        return 1;
    }

    hash_object(argv[file_arg], write);
    return 0;
}

int cmd_cat_file(int argc, char const *argv[])
{
    // minigit cat-file (-t | -s | -p) <object>
    if (argc < 4)
    {
        std::cerr << "usage: minigit cat-file (-t | -s | -p) <object>\n";
        return 1;
    }
    cat_file(argv[2], argv[3]);
    return 0;
}

int cmd_write_tree(int /*argc*/, char const * /*argv*/[])
{
    write_tree();
    return 0;
}

int cmd_commit(int argc, char const *argv[])
{
    std::string message;
    std::string author = "MiniGit User <user@minigit>";

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-m" && i + 1 < argc)
            message = argv[++i];
        else if (arg == "--author" && i + 1 < argc)
            author = argv[++i];
    }

    if (message.empty())
    {
        std::cerr << "usage: minigit commit -m <message> [--author <author>]\n";
        return 1;
    }

    commit(message, author);
    return 0;
}

int cmd_log(int /*argc*/, char const * /*argv*/[])
{
    log();
    return 0;
}

int cmd_show(int argc, char const *argv[])
{
    return show_command(argc, argv);
}


int cmd_status(int /*argc*/, char const * /*argv*/[])
{
    status();
    return 0;
}

int cmd_clean(int argc, char const *argv[])
{
    return clean_command(argc, argv);
}

int cmd_diff(int argc, char const *argv[])
{
    // minigit diff [--cached] [<path>...]
    bool cached = false;
    std::vector<std::string> paths;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--cached" || arg == "--staged")
            cached = true;
        else
            paths.emplace_back(arg);
    }

    diff_command(cached, paths);
    return 0;
}

int cmd_branch(int argc, char const *argv[])
{
    // minigit branch              → list
    // minigit branch <name>       → create
    // minigit branch -d <name>    → delete
    bool del = false;
    std::string name;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-d" && i + 1 < argc)
        {
            del = true;
            name = argv[++i];
        }
        else
        {
            name = arg;
        }
    }

    branch_command(name, del);
    return 0;
}

int cmd_checkout(int argc, char const *argv[])
{
    if (argc < 3)
    {
        std::cerr << "usage: minigit checkout <branch-or-sha>\n";
        return 1;
    }
    checkout_command(argv[2]);
    return 0;
}

int cmd_switch(int argc, char const *argv[])
{
    // minigit switch <branch>
    // minigit switch -c <new-branch>
    bool create = false;
    std::string branch;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc)
        {
            create = true;
            branch = argv[++i];
        }
        else
        {
            branch = arg;
        }
    }

    if (branch.empty())
    {
        std::cerr << "usage: minigit switch [-c] <branch>\n";
        return 1;
    }

    switch_command(branch, create);
    return 0;
}

int cmd_tag(int argc, char const *argv[])
{
    // minigit tag                         → list all tags
    // minigit tag <name>                  → lightweight tag at HEAD
    // minigit tag -a <name> -m <message>  → annotated tag
    // minigit tag -d <name>               → delete tag
    bool annotated = false;
    bool del = false;
    std::string tag_name;
    std::string tag_msg;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-a")
        {
            annotated = true;
        }
        else if (arg == "-m" && i + 1 < argc)
        {
            tag_msg = argv[++i];
        }
        else if (arg == "-d" && i + 1 < argc)
        {
            del = true;
            tag_name = argv[++i];
        }
        else if (arg[0] != '-')
        {
            tag_name = arg;
        }
    }

    tag_command(tag_name, annotated, tag_msg, del);
    return 0;
}

int cmd_reset(int argc, char const *argv[])
{
    // minigit reset [--soft | --mixed | --hard] <commit>
    // Default mode: --mixed
    std::string reset_mode;
    std::string reset_target;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--soft" || arg == "--mixed" || arg == "--hard")
            reset_mode = arg;
        else
            reset_target = arg;
    }

    if (reset_target.empty())
    {
        std::cerr << "usage: minigit reset [--soft | --mixed | --hard] <commit>\n";
        return 1;
    }

    // Default to --mixed when no mode flag is given.
    if (reset_mode.empty())
        reset_mode = "--mixed";

    reset_command(reset_mode, reset_target);
    return 0;
}

int cmd_merge(int argc, char const *argv[])
{
    // minigit merge <branch> [--author <author>]
    std::string merge_branch;
    std::string merge_author = "MiniGit User <user@minigit>";

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--author" && i + 1 < argc)
            merge_author = argv[++i];
        else if (arg[0] != '-')
            merge_branch = arg;
    }

    if (merge_branch.empty())
    {
        std::cerr << "usage: minigit merge <branch> [--author <author>]\n";
        return 1;
    }

    merge_command(merge_branch, merge_author);
    return 0;
}

int cmd_revert(int argc, char const *argv[])
{
    // minigit revert <commit> [--author <author>]
    std::string revert_target;
    std::string revert_author = "MiniGit User <user@minigit>";

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--author" && i + 1 < argc)
            revert_author = argv[++i];
        else if (arg[0] != '-')
            revert_target = arg;
    }

    if (revert_target.empty())
    {
        std::cerr << "usage: minigit revert <commit> [--author <author>]\n";
        return 1;
    }

    revert_command(revert_target, revert_author);
    return 0;
}

int cmd_cherry_pick(int argc, char const *argv[])
{
    // minigit cherry-pick [-n | --no-commit] [--author <author>] [-m <parent>] <commit>
    bool no_commit = false;
    std::string author;
    int parent_index = 1;
    std::string target;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-n" || arg == "--no-commit")
        {
            no_commit = true;
        }
        else if (arg == "--author" && i + 1 < argc)
        {
            author = argv[++i];
        }
        else if (arg == "-m" && i + 1 < argc)
        {
            try
            {
                parent_index = std::stoi(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "error: invalid parent number\n";
                return 1;
            }
        }
        else if (arg[0] != '-')
        {
            target = arg;
        }
    }

    if (target.empty())
    {
        std::cerr << "usage: minigit cherry-pick [-n] [--author <author>] [-m <parent>] <commit>\n";
        return 1;
    }

    cherry_pick_command(target, author, no_commit, parent_index);
    return 0;
}

int cmd_rebase(int argc, char const *argv[])
{
    return rebase_command(argc, argv);
}

int cmd_stash(int argc, char const *argv[])
{
    // minigit stash [push]            → save working state, restore HEAD
    // minigit stash list              → list all stash entries
    // minigit stash pop  [stash@{N}]  → apply + drop Nth entry
    // minigit stash drop [stash@{N}]  → discard Nth entry
    // minigit stash show [stash@{N}]  → show files in Nth entry
    std::string sub = "push";           // default subcommand
    std::string ref = "stash@{0}";      // default stash reference

    if (argc >= 3) sub = argv[2];
    if (argc >= 4) ref = argv[3];

    stash_command(sub, ref);
    return 0;
}

int cmd_remote(int argc, char const *argv[])
{
    // minigit remote                          → list
    // minigit remote -v                       → list verbose
    // minigit remote add <name> <url>
    // minigit remote remove <name>
    std::string sub2, rname, rurl;
    if (argc >= 3) sub2  = argv[2];
    if (argc >= 4) rname = argv[3];
    if (argc >= 5) rurl  = argv[4];
    remote_command(sub2, rname, rurl);
    return 0;
}

int cmd_clone(int argc, char const *argv[])
{
    // minigit clone <src-path> [<dest-dir>]
    if (argc < 3)
    {
        std::cerr << "usage: minigit clone <repository> [<directory>]\n";
        return 1;
    }
    const std::string dest = (argc >= 4) ? argv[3] : "";
    clone_command(argv[2], dest);
    return 0;
}

int cmd_fetch(int argc, char const *argv[])
{
    // minigit fetch [<remote>]
    const std::string rname2 = (argc >= 3) ? argv[2] : "origin";
    fetch_command(rname2);
    return 0;
}

int cmd_push(int argc, char const *argv[])
{
    // minigit push [<remote> [<branch>]]
    const std::string rname3 = (argc >= 3) ? argv[2] : "origin";
    const std::string branch = (argc >= 4) ? argv[3] : "";
    push_command(rname3, branch);
    return 0;
}

int cmd_pull(int argc, char const *argv[])
{
    // minigit pull [<remote> [<branch>]]
    const std::string rname4 = (argc >= 3) ? argv[2] : "origin";
    const std::string branch = (argc >= 4) ? argv[3] : "";
    pull_command(rname4, branch);
    return 0;
}

int cmd_ls_files(int argc, char const *argv[])
{
    return ls_files_command(argc, argv);
}

int cmd_ls_tree(int argc, char const *argv[])
{
    return ls_tree_command(argc, argv);
}

int cmd_repack(int argc, char const *argv[])
{
    return minigit::storage::repack_command(argc, argv);
}

int cmd_verify_pack(int argc, char const *argv[])
{
    return minigit::storage::verify_pack_command(argc, argv);
}

int cmd_worktree(int argc, char const *argv[])
{
    return minigit::worktree::worktree_command(argc, argv);
}

int cmd_submodule(int argc, char const *argv[])
{
    return submodule_command(argc, argv);
}

int cmd_bisect(int argc, char const *argv[])
{
    return minigit::bisect::bisect_command(argc, argv);
}

int cmd_version(int /*argc*/, char const * /*argv*/[])
{
    std::cout << "minigit version " << minigit::core::MINIGIT_VERSION << "\n";
    return 0;
}

int cmd_install(int argc, char const *argv[])
{
    return minigit::install::install_command(argc, argv);
}

int cmd_update(int argc, char const *argv[])
{
    return minigit::update::update_command(argc, argv);
}

int cmd_help(int /*argc*/, char const * /*argv*/[])
{
    std::cout << "usage: minigit [--version] [--help] [-C <path>] [-c <name>=<value>]\n"
              << "               [--trace] [--trace=<file>] [--log-level=<level>]\n"
              << "               <command> [<args>]\n\n"
              << "These are common MiniGit commands used in various situations:\n\n"
              << "start a working area\n"
              << "   init              Create an empty MiniGit repository or reinitialize an existing one\n"
              << "   clone             Clone a repository into a new directory\n\n"
              << "work on the current change\n"
              << "   add               Add file contents to the staging index\n"
              << "   status            Show the working tree status\n"
              << "   diff              Show changes between commits, commit and working tree, etc\n"
              << "   reset             Reset current HEAD to the specified state\n"
              << "   clean             Remove untracked files from the working tree\n"
              << "   stash             Stash the changes in a dirty working directory away\n\n"
              << "examine the history and state\n"
              << "   log               Show commit logs\n"
              << "   show              Show various types of objects (commits, tags, trees, blobs)\n\n"
              << "grow, mark and tweak your common history\n"
              << "   branch            List, create, or delete branches\n"
              << "   checkout          Switch branches or restore working tree files\n"
              << "   switch            Switch branches\n"
              << "   commit            Record changes to the repository\n"
              << "   merge             Join two or more development histories together\n"
              << "   rebase            Reapply commits on top of another base tip\n"
              << "   tag               Create, list, delete or verify a tag object\n"
              << "   cherry-pick       Apply the changes introduced by some existing commits\n"
              << "   revert            Revert some existing commits\n"
              << "   bisect            Use binary search to find the commit that introduced a bug\n\n"
              << "collaborate and manage remotes\n"
              << "   remote            Manage set of tracked repositories\n"
              << "   fetch             Download objects and refs from another repository\n"
              << "   pull              Fetch from and integrate with another repository or branch\n"
              << "   push              Update remote refs along with associated objects\n\n"
              << "multiple worktrees & nested repositories\n"
              << "   worktree          Manage multiple working trees attached to the same repository\n"
              << "   submodule         Initialize, update or inspect submodules\n\n"
              << "low-level plumbing & storage\n"
              << "   hash-object       Compute object ID and optionally creates a blob from a file\n"
              << "   cat-file          Provide content or type/size information for repository objects\n"
              << "   write-tree        Create a tree object from the current index\n"
              << "   ls-files          Show information about files in the index and working tree\n"
              << "   ls-tree           List the contents of a tree object\n"
              << "   repack            Pack unpacked objects in a repository\n"
              << "   verify-pack       Validate packed MiniGit archive files\n\n"
              << "system & tools\n"
              << "   version           Display MiniGit version information\n"
              << "   install           Install MiniGit binary to user or system PATH\n"
              << "   update            Check for and apply self-updates\n";
    return 0;
}

} // namespace

namespace minigit::cli {

int run(int argc, char const *argv[])
{
    minigit::core::Logger::instance().init_from_env();

    int arg_idx = 1;
    while (arg_idx < argc)
    {
        std::string_view arg = argv[arg_idx];
        if (arg == "--trace")
        {
            minigit::core::Logger::instance().set_level(minigit::core::LogLevel::TRACE);
            arg_idx++;
        }
        else if (arg.starts_with("--trace="))
        {
            auto path = arg.substr(8);
            if (!path.empty())
            {
                minigit::core::Logger::instance().set_output_file(std::string(path));
            }
            minigit::core::Logger::instance().set_level(minigit::core::LogLevel::TRACE);
            arg_idx++;
        }
        else if (arg.starts_with("--log-level="))
        {
            auto lvl = arg.substr(12);
            minigit::core::Logger::instance().set_level(minigit::core::log_level_from_string(lvl));
            arg_idx++;
        }
        else
        {
            break;
        }
    }

    if (arg_idx >= argc)
    {
        return cmd_help(argc, argv);
    }

    static const std::unordered_map<std::string, CommandHandler> commands = {
        {"help", cmd_help},
        {"--help", cmd_help},
        {"-h", cmd_help},
        {"version", cmd_version},
        {"--version", cmd_version},
        {"-v", cmd_version},
        {"init", cmd_init},
        {"add", cmd_add},
        {"hash-object", cmd_hash_object},
        {"cat-file", cmd_cat_file},
        {"write-tree", cmd_write_tree},
        {"commit", cmd_commit},
        {"log", cmd_log},
        {"show", cmd_show},
        {"status", cmd_status},
        {"clean", cmd_clean},
        {"ls-files", cmd_ls_files},
        {"ls-tree", cmd_ls_tree},
        {"diff", cmd_diff},
        {"branch", cmd_branch},
        {"checkout", cmd_checkout},
        {"switch", cmd_switch},
        {"tag", cmd_tag},
        {"reset", cmd_reset},
        {"merge", cmd_merge},
        {"revert", cmd_revert},
        {"cherry-pick", cmd_cherry_pick},
        {"rebase", cmd_rebase},
        {"stash", cmd_stash},
        {"remote", cmd_remote},
        {"clone", cmd_clone},
        {"fetch", cmd_fetch},
        {"push", cmd_push},
        {"pull", cmd_pull},
        {"repack", cmd_repack},
        {"verify-pack", cmd_verify_pack},
        {"worktree", cmd_worktree},
        {"submodule", cmd_submodule},
        {"bisect", cmd_bisect},
        {"install", cmd_install},
        {"update", cmd_update}
    };

    std::vector<char const*> shifted_argv;
    int effective_argc = argc;
    char const** effective_argv = argv;

    if (arg_idx > 1)
    {
        shifted_argv.reserve(argc - arg_idx + 1);
        shifted_argv.push_back(argv[0]);
        for (int i = arg_idx; i < argc; ++i)
        {
            shifted_argv.push_back(argv[i]);
        }
        effective_argc = static_cast<int>(shifted_argv.size());
        effective_argv = shifted_argv.data();
    }

    const std::string command = effective_argv[1];
    const auto it = commands.find(command);
    if (it == commands.end())
    {
        std::cerr << "Unknown command: " << command << '\n';
        return 1;
    }

    LOG_DEBUG("cli", "executing command '" << command << "' with " << (effective_argc - 2) << " arguments");
    int result = it->second(effective_argc, effective_argv);
    LOG_DEBUG("cli", "command '" << command << "' completed with exit code " << result);

    if (result == 0)
    {
        minigit::update::UpdateNotifier::instance().check_and_notify(command);
    }

    return result;
}

} // namespace minigit::cli
