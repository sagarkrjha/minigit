#include "show.h"

#include "diff/diff_engine.h"
#include "repository/repository.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string trim_trailing(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

std::string read_text_file(const fs::path& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return trim_trailing(ss.str());
}

bool is_all_hex(const std::string& s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
        return std::isxdigit(static_cast<unsigned char>(c));
    });
}

// Extract the object type from "<type> <size>\0<body>"
std::string get_object_type(const std::string& raw)
{
    const auto space_pos = raw.find(' ');
    if (space_pos == std::string::npos)
        return "";
    return raw.substr(0, space_pos);
}

// Resolve user-supplied base target to an initial object SHA.
std::string resolve_base_ref(const fs::path& git_dir, const std::string& target, std::string& error)
{
    // 1. HEAD
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

    // 2. Branch ref
    const fs::path branch_path = git_dir / "refs" / "heads" / target;
    if (fs::exists(branch_path))
    {
        const std::string val = read_text_file(branch_path);
        if (!val.empty())
            return val;
    }

    // 3. Tag ref
    const fs::path tag_path = git_dir / "refs" / "tags" / target;
    if (fs::exists(tag_path))
    {
        const std::string val = read_text_file(tag_path);
        if (!val.empty())
            return val;
    }

    // 4. Exact 64-character SHA
    if (target.size() == 64 && is_all_hex(target))
    {
        const fs::path obj_path = git_dir / "objects" / target.substr(0, 2) / target.substr(2);
        if (fs::exists(obj_path))
            return target;
    }

    // 5. Short SHA prefix (>= 4 hex chars and < 64)
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

// Parse possible ancestry specifiers (~N or ^)
std::string resolve_target(const fs::path& git_dir, ObjectDatabase& db, const std::string& raw_target, std::string& error)
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

    // Walk ancestor steps if ~N or ^ was specified
    for (int i = 0; i < ancestor_count; ++i)
    {
        std::string raw_obj;
        try
        {
            raw_obj = db.read(current_sha);
        }
        catch (const std::exception& e)
        {
            error = "fatal: unable to read object " + current_sha + ": " + e.what();
            return "";
        }

        std::string type = get_object_type(raw_obj);
        if (type == "tag")
        {
            // Dereference tag to find target commit
            const size_t null_pos = raw_obj.find('\0');
            if (null_pos != std::string::npos)
            {
                std::istringstream iss(raw_obj.substr(null_pos + 1));
                std::string line;
                while (std::getline(iss, line))
                {
                    if (line.rfind("object ", 0) == 0)
                    {
                        current_sha = trim_trailing(line.substr(7));
                        raw_obj = db.read(current_sha);
                        type = get_object_type(raw_obj);
                        break;
                    }
                }
            }
        }

        if (type != "commit")
        {
            error = "fatal: object " + current_sha + " is a " + type + ", not a commit";
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

    return current_sha;
}

// Diff structure for diffstat computation
struct FileDiffStat
{
    std::string path;
    int insertions{0};
    int deletions{0};
};

} // namespace

ShowResult perform_show(
    const fs::path& repo_root,
    const std::string& target,
    ShowFormat format)
{
    ShowResult result;
    const fs::path git_dir = repo_root / ".minigit";

    if (!fs::exists(git_dir))
    {
        result.error_message = "fatal: not a git repository (or any of the parent directories): .minigit";
        return result;
    }

    ObjectDatabase db(git_dir / "objects");
    std::string resolve_err;
    std::string sha = resolve_target(git_dir, db, target, resolve_err);

    if (sha.empty())
    {
        result.error_message = resolve_err;
        return result;
    }

    result.object_sha = sha;

    std::string raw;
    try
    {
        raw = db.read(sha);
    }
    catch (const std::exception& e)
    {
        result.error_message = "fatal: could not read object " + sha + ": " + e.what();
        return result;
    }

    std::string type = get_object_type(raw);
    result.object_type = type;

    std::ostringstream out;

    // 1. Tag object: Show tag metadata and dereference to target object
    if (type == "tag")
    {
        const size_t null_pos = raw.find('\0');
        std::string tag_name, tagger, timestamp, message, target_obj;
        if (null_pos != std::string::npos)
        {
            std::istringstream body(raw.substr(null_pos + 1));
            std::string line;
            bool past_blank = false;
            while (std::getline(body, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                if (!past_blank)
                {
                    if (line.empty())
                    {
                        past_blank = true;
                        continue;
                    }
                    if (line.rfind("object ", 0) == 0)
                        target_obj = line.substr(7);
                    else if (line.rfind("tag ", 0) == 0)
                        tag_name = line.substr(4);
                    else if (line.rfind("tagger ", 0) == 0)
                    {
                        const auto last_sp = line.rfind(' ');
                        if (last_sp != std::string::npos)
                        {
                            timestamp = line.substr(last_sp + 1);
                            tagger = line.substr(7, last_sp - 7);
                        }
                        else
                        {
                            tagger = line.substr(7);
                        }
                    }
                }
                else
                {
                    if (!message.empty())
                        message += '\n';
                    message += line;
                }
            }
        }

        out << "tag " << tag_name << '\n';
        if (!tagger.empty())
            out << "Tagger: " << tagger << '\n';
        if (!timestamp.empty())
            out << "Date:   " << timestamp << '\n';
        out << '\n';
        if (!message.empty())
            out << "    " << message << "\n\n";

        // Now recursively show the target object
        if (!target_obj.empty())
        {
            ShowResult inner = perform_show(repo_root, target_obj, format);
            if (inner.success)
                out << inner.output;
            else
                out << "error showing tagged object: " << inner.error_message << '\n';
        }

        result.output = out.str();
        result.success = true;
        return result;
    }

    // 2. Commit object: print commit header and diff against parent
    if (type == "commit")
    {
        const ParsedCommit c = parse_commit(raw);

        out << "commit " << sha << '\n';
        out << "Author: " << c.author << '\n';
        out << "Date:   " << c.timestamp << '\n';
        out << '\n';

        std::istringstream msg_ss(c.message);
        std::string mline;
        while (std::getline(msg_ss, mline))
        {
            if (!mline.empty() && mline.back() == '\r')
                mline.pop_back();
            out << "    " << mline << '\n';
        }
        out << '\n';

        // Load parent tree
        std::map<std::string, std::string> parent_tree;
        if (!c.parent_ids.empty())
        {
            try
            {
                const std::string parent_raw = db.read(c.parent_ids[0]);
                const ParsedCommit parent_commit = parse_commit(parent_raw);
                const ParsedTree ptree = parse_tree(db.read(parent_commit.tree_id));
                for (const auto& entry : ptree.entries)
                    parent_tree[entry.name] = entry.id;
            }
            catch (...) {}
        }

        // Load commit tree
        std::map<std::string, std::string> current_tree;
        try
        {
            const ParsedTree ctree = parse_tree(db.read(c.tree_id));
            for (const auto& entry : ctree.entries)
                current_tree[entry.name] = entry.id;
        }
        catch (...) {}

        // Collect all distinct paths in sorted order
        std::map<std::string, bool> all_paths;
        for (const auto& [p, _] : parent_tree)
            all_paths[p] = true;
        for (const auto& [p, _] : current_tree)
            all_paths[p] = true;

        std::vector<FileDiffStat> stats;
        std::string patches;

        for (const auto& [path, _] : all_paths)
        {
            const auto it_old = parent_tree.find(path);
            const auto it_new = current_tree.find(path);

            const bool in_old = (it_old != parent_tree.end());
            const bool in_new = (it_new != current_tree.end());

            if (in_old && in_new && it_old->second == it_new->second)
                continue; // unchanged

            if (format == ShowFormat::NameOnly)
            {
                out << path << '\n';
                continue;
            }

            std::string old_content;
            std::string new_content;

            if (in_old)
            {
                try { old_content = strip_object_header(db.read(it_old->second)); }
                catch (...) {}
            }
            if (in_new)
            {
                try { new_content = strip_object_header(db.read(it_new->second)); }
                catch (...) {}
            }

            const auto edits = lcs_diff(split_lines(old_content), split_lines(new_content));

            if (format == ShowFormat::Stat)
            {
                FileDiffStat fstat;
                fstat.path = path;
                for (const auto& e : edits)
                {
                    if (e.type == EditType::Add)    ++fstat.insertions;
                    if (e.type == EditType::Remove) ++fstat.deletions;
                }
                stats.push_back(fstat);
            }
            else // Default unified diff
            {
                std::string patch;
                if (!in_old)
                    patch = format_unified_diff("/dev/null", path, edits);
                else if (!in_new)
                    patch = format_unified_diff(path, "/dev/null", edits);
                else
                    patch = format_unified_diff(path, path, edits);

                patches += patch;
            }
        }

        if (format == ShowFormat::Stat)
        {
            int total_ins = 0;
            int total_del = 0;
            size_t max_path_len = 0;

            for (const auto& s : stats)
            {
                max_path_len = std::max(max_path_len, s.path.size());
                total_ins += s.insertions;
                total_del += s.deletions;
            }

            for (const auto& s : stats)
            {
                const int total_changes = s.insertions + s.deletions;
                std::string graph;
                // Generate up to 20 graph symbols
                const int max_graph = 20;
                int ins_bar = s.insertions;
                int del_bar = s.deletions;
                if (total_changes > max_graph)
                {
                    ins_bar = (s.insertions * max_graph) / total_changes;
                    del_bar = max_graph - ins_bar;
                }
                graph.append(ins_bar, '+');
                graph.append(del_bar, '-');

                out << " " << std::left << std::setw(static_cast<int>(max_path_len)) << s.path
                    << " | " << std::right << std::setw(3) << total_changes << " "
                    << graph << '\n';
            }

            if (!stats.empty())
            {
                out << " " << stats.size() << " file" << (stats.size() == 1 ? "" : "s") << " changed";
                if (total_ins > 0)
                    out << ", " << total_ins << " insertion" << (total_ins == 1 ? "" : "s") << "(+)";
                if (total_del > 0)
                    out << ", " << total_del << " deletion" << (total_del == 1 ? "" : "s") << "(-)";
                out << '\n';
            }
        }
        else if (format == ShowFormat::Default)
        {
            out << patches;
        }

        result.output = out.str();
        result.success = true;
        return result;
    }

    // 3. Tree object: Print tree entries
    if (type == "tree")
    {
        const ParsedTree tree = parse_tree(raw);
        for (const auto& entry : tree.entries)
        {
            const std::string entry_type = (entry.mode == "040000") ? "tree" : "blob";
            out << entry.mode << ' ' << entry_type << ' ' << entry.id << "    " << entry.name << '\n';
        }
        result.output = out.str();
        result.success = true;
        return result;
    }

    // 4. Blob object: Print content
    if (type == "blob")
    {
        result.output = strip_object_header(raw);
        result.success = true;
        return result;
    }

    result.error_message = "fatal: unknown object type '" + type + "' for " + sha;
    return result;
}

int show_command(int argc, char const *argv[])
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    std::string target = "HEAD";
    ShowFormat format = ShowFormat::Default;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--stat")
        {
            format = ShowFormat::Stat;
        }
        else if (arg == "--name-only")
        {
            format = ShowFormat::NameOnly;
        }
        else if (!arg.empty() && arg[0] != '-')
        {
            target = arg;
        }
        else
        {
            std::cerr << "error: unknown option: " << arg << '\n';
            std::cerr << "usage: minigit show [--stat | --name-only] [<object>]\n";
            return 1;
        }
    }

    ShowResult res = perform_show(repo.root(), target, format);
    if (!res.success)
    {
        std::cerr << res.error_message << '\n';
        return 1;
    }

    std::cout << res.output;
    return 0;
}
