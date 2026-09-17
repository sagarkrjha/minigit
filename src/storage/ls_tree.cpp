#include "ls_tree.h"

#include "object_database.h"
#include "object_parser.h"
#include "repository/repository.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool is_all_hex(const std::string& s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isxdigit(c);
    });
}

std::string read_text_file(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f)
        return "";
    std::string s;
    std::getline(f, s);
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    return s;
}

std::string get_object_type(const std::string& raw)
{
    const auto space_pos = raw.find(' ');
    if (space_pos == std::string::npos)
        return "";
    return raw.substr(0, space_pos);
}

std::string resolve_base_ref(const fs::path& git_dir, const std::string& target, std::string& error)
{
    if (target.empty() || target == "HEAD")
    {
        const fs::path head_path = git_dir / "HEAD";
        if (!fs::exists(head_path))
        {
            error = "fatal: not a valid repository (missing HEAD)";
            return "";
        }
        std::string head_content = read_text_file(head_path);
        if (head_content.rfind("ref: ", 0) == 0)
        {
            const std::string ref_rel = head_content.substr(5);
            const fs::path ref_path = git_dir / ref_rel;
            if (!fs::exists(ref_path))
            {
                error = "fatal: your current branch has no commits yet";
                return "";
            }
            head_content = read_text_file(ref_path);
        }
        if (head_content.empty())
        {
            error = "fatal: your current branch has no commits yet";
            return "";
        }
        return head_content;
    }

    // Branch ref
    const fs::path branch_path = git_dir / "refs" / "heads" / target;
    if (fs::exists(branch_path))
    {
        const std::string val = read_text_file(branch_path);
        if (!val.empty())
            return val;
    }

    // Tag ref
    const fs::path tag_path = git_dir / "refs" / "tags" / target;
    if (fs::exists(tag_path))
    {
        const std::string val = read_text_file(tag_path);
        if (!val.empty())
            return val;
    }

    // Exact 64-char SHA
    if (target.size() == 64 && is_all_hex(target))
    {
        const fs::path obj_path = git_dir / "objects" / target.substr(0, 2) / target.substr(2);
        if (fs::exists(obj_path))
            return target;
    }

    // Short SHA prefix (>= 4 and < 64)
    if (target.size() >= 4 && target.size() < 64 && is_all_hex(target))
    {
        const fs::path prefix_dir = git_dir / "objects" / target.substr(0, 2);
        if (fs::exists(prefix_dir))
        {
            const std::string rest = target.substr(2);
            std::vector<std::string> matches;
            for (const auto& entry : fs::directory_iterator(prefix_dir))
            {
                if (entry.is_regular_file())
                {
                    const std::string fname = entry.path().filename().string();
                    if (fname.rfind(rest, 0) == 0)
                        matches.push_back(target.substr(0, 2) + fname);
                }
            }
            if (matches.size() == 1)
                return matches[0];
            if (matches.size() > 1)
            {
                error = "error: short SHA prefix '" + target + "' is ambiguous";
                return "";
            }
        }
    }

    error = "fatal: Not a valid object name " + target;
    return "";
}

std::string resolve_to_tree_sha(const fs::path& git_dir, ObjectDatabase& db, const std::string& raw_target, std::string& error)
{
    std::string base_name = raw_target;
    int ancestor_count = 0;

    const auto tilde_pos = raw_target.find('~');
    const auto caret_pos = raw_target.find('^');

    if (tilde_pos != std::string::npos)
    {
        base_name = raw_target.substr(0, tilde_pos);
        const std::string num_str = raw_target.substr(tilde_pos + 1);
        ancestor_count = num_str.empty() ? 1 : std::max(0, std::stoi(num_str));
    }
    else if (caret_pos != std::string::npos)
    {
        base_name = raw_target.substr(0, caret_pos);
        const std::string num_str = raw_target.substr(caret_pos + 1);
        ancestor_count = num_str.empty() ? 1 : std::max(0, std::stoi(num_str));
    }

    std::string current_sha = resolve_base_ref(git_dir, base_name, error);
    if (current_sha.empty())
        return "";

    for (int i = 0; i < ancestor_count; ++i)
    {
        std::string raw_obj;
        try
        {
            raw_obj = db.read(current_sha);
        }
        catch (const std::exception& e)
        {
            error = "fatal: " + std::string(e.what());
            return "";
        }

        const std::string otype = get_object_type(raw_obj);
        if (otype != "commit")
        {
            error = "fatal: object " + current_sha + " is a " + otype + ", not a commit";
            return "";
        }

        const ParsedCommit c = parse_commit(raw_obj);
        if (c.parent_ids.empty())
        {
            error = "fatal: commit " + current_sha + " has no parents";
            return "";
        }
        current_sha = c.parent_ids[0];
    }

    // Now current_sha is our target object. Read it and resolve to tree.
    std::string raw;
    try
    {
        raw = db.read(current_sha);
    }
    catch (const std::exception& e)
    {
        error = "fatal: " + std::string(e.what());
        return "";
    }

    std::string otype = get_object_type(raw);

    // If tag, unwrap it
    if (otype == "tag")
    {
        const std::string body = strip_object_header(raw);
        std::istringstream tag_stream(body);
        std::string line;
        std::string target_obj;
        std::string target_type;
        while (std::getline(tag_stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.rfind("object ", 0) == 0)
                target_obj = line.substr(7);
            else if (line.rfind("type ", 0) == 0)
                target_type = line.substr(5);
        }

        if (!target_obj.empty())
        {
            current_sha = target_obj;
            try
            {
                raw = db.read(current_sha);
                otype = get_object_type(raw);
            }
            catch (const std::exception& e)
            {
                error = "fatal: " + std::string(e.what());
                return "";
            }
        }
    }

    if (otype == "tree")
    {
        return current_sha;
    }

    if (otype == "commit")
    {
        const ParsedCommit c = parse_commit(raw);
        return c.tree_id;
    }

    error = "fatal: not a tree object";
    return "";
}

bool matches_filters(const std::string& rel_path, bool is_dir, const std::vector<std::string>& filters)
{
    if (filters.empty())
        return true;

    for (const auto& f : filters)
    {
        std::string norm = f;
        while (norm.size() > 1 && (norm.back() == '/' || norm.back() == '\\'))
            norm.pop_back();

        if (norm == "." || norm.empty())
            return true;

        if (rel_path == norm || rel_path.rfind(norm + "/", 0) == 0)
            return true;

        // If this entry is a directory, check if norm is inside this directory
        if (is_dir && norm.rfind(rel_path + "/", 0) == 0)
            return true;
    }

    return false;
}

void traverse_tree(
    ObjectDatabase& db,
    const std::string& current_tree_sha,
    const std::string& current_prefix,
    const LsTreeOptions& options,
    std::vector<LsTreeEntry>& out)
{
    std::string raw;
    try
    {
        raw = db.read(current_tree_sha);
    }
    catch (...)
    {
        return;
    }

    const ParsedTree tree = parse_tree(raw);
    for (const auto& entry : tree.entries)
    {
        const bool is_dir = (entry.mode == "040000");
        const std::string type = is_dir ? "tree" : "blob";
        const std::string full_path = current_prefix.empty() ? entry.name : (current_prefix + entry.name);

        if (is_dir)
        {
            if (options.recursive)
            {
                if (options.tree_only || options.show_trees)
                {
                    if (matches_filters(full_path, true, options.paths))
                    {
                        out.push_back({entry.mode, type, entry.id, full_path});
                    }
                }
                // Recurse into child tree
                traverse_tree(db, entry.id, full_path + "/", options, out);
            }
            else
            {
                if (matches_filters(full_path, true, options.paths))
                {
                    out.push_back({entry.mode, type, entry.id, full_path});
                }
            }
        }
        else
        {
            if (!options.tree_only)
            {
                if (matches_filters(full_path, false, options.paths))
                {
                    out.push_back({entry.mode, type, entry.id, full_path});
                }
            }
        }
    }
}

} // namespace

LsTreeResult perform_ls_tree(
    const fs::path& repo_root,
    const LsTreeOptions& options)
{
    LsTreeResult result;
    const fs::path git_dir = repo_root / ".minigit";

    if (!fs::exists(git_dir))
    {
        result.error_message = "fatal: not a git repository (or any of the parent directories): .minigit";
        return result;
    }

    if (options.name_only && options.object_only)
    {
        result.error_message = "fatal: --name-only and --object-only cannot be used together";
        return result;
    }

    if (options.tree_ish.empty())
    {
        result.error_message = "fatal: Not a valid object name ";
        return result;
    }

    ObjectDatabase db(git_dir / "objects");
    std::string resolve_err;
    const std::string tree_sha = resolve_to_tree_sha(git_dir, db, options.tree_ish, resolve_err);
    if (tree_sha.empty())
    {
        result.error_message = resolve_err.empty() ? ("fatal: Not a valid object name " + options.tree_ish) : resolve_err;
        return result;
    }

    traverse_tree(db, tree_sha, "", options, result.entries);

    std::ostringstream out;
    for (const auto& entry : result.entries)
    {
        if (options.name_only)
        {
            out << entry.path << '\n';
        }
        else if (options.object_only)
        {
            out << entry.sha << '\n';
        }
        else
        {
            // Pad mode to 6 chars if needed (e.g. "40000" -> "040000")
            std::string mode_str = entry.mode;
            if (mode_str.size() < 6)
                mode_str = std::string(6 - mode_str.size(), '0') + mode_str;

            out << mode_str << " " << entry.type << " " << entry.sha << "\t" << entry.path << '\n';
        }
    }

    result.output = out.str();
    result.success = true;
    return result;
}

int ls_tree_command(int argc, char const *argv[])
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    LsTreeOptions options;
    bool tree_ish_found = false;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--name-only")
            options.name_only = true;
        else if (arg == "--object-only")
            options.object_only = true;
        else if (arg.rfind("--", 0) == 0)
        {
            std::cerr << "error: unknown option: " << arg << '\n';
            std::cerr << "usage: minigit ls-tree [-d] [-r] [-t] [--name-only] [--object-only] <tree-ish> [<path>...]\n";
            return 1;
        }
        else if (arg.rfind("-", 0) == 0 && arg.size() > 1)
        {
            for (size_t c = 1; c < arg.size(); ++c)
            {
                switch (arg[c])
                {
                    case 'd': options.tree_only = true; break;
                    case 'r': options.recursive = true; break;
                    case 't': options.show_trees = true; break;
                    default:
                        std::cerr << "error: unknown switch: -" << arg[c] << '\n';
                        std::cerr << "usage: minigit ls-tree [-d] [-r] [-t] [--name-only] [--object-only] <tree-ish> [<path>...]\n";
                        return 1;
                }
            }
        }
        else
        {
            if (!tree_ish_found)
            {
                options.tree_ish = arg;
                tree_ish_found = true;
            }
            else
            {
                options.paths.push_back(arg);
            }
        }
    }

    if (!tree_ish_found)
    {
        std::cerr << "usage: minigit ls-tree [-d] [-r] [-t] [--name-only] [--object-only] <tree-ish> [<path>...]\n";
        return 1;
    }

    LsTreeResult res = perform_ls_tree(repo.root(), options);
    if (!res.success)
    {
        std::cerr << res.error_message << '\n';
        return 1;
    }

    std::cout << res.output;
    return 0;
}
