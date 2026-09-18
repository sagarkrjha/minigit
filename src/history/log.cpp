#include "log.h"

#include "storage/object_database.h"
#include "repository/repository.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Internal helpers (duplicated from commit.cpp — shared later via utils)
// ---------------------------------------------------------------------------

static std::string read_text(const std::filesystem::path &path)
{
    std::ifstream f(path);
    if (!f)
        return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string trim(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

static std::string resolve_head(const std::filesystem::path &git_dir)
{
    return Repository::resolve_head_from_dir(git_dir);
}

// Parse the raw commit object body and pretty-print it.
// Raw format (after "commit <size>\0"):
//   tree <id>
//   [parent <id>]
//   author <name> <ts>
//   committer <name> <ts>
//   (blank line)
//   <message>
static void print_commit(const std::string &sha, const std::string &raw)
{
    // Skip the object header "commit <size>\0"
    const auto null_pos = raw.find('\0');
    if (null_pos == std::string::npos)
    {
        std::cerr << "error: malformed object " << sha << '\n';
        return;
    }

    std::istringstream body(raw.substr(null_pos + 1));
    std::string line;

    std::string tree_id, author, timestamp, message;
    std::vector<std::string> parents;
    bool in_message = false;
    bool past_blank = false;

    while (std::getline(body, line))
    {
        // Remove any trailing \r
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (!past_blank)
        {
            if (line.empty())
            {
                past_blank = true;
                continue;
            }

            if (line.substr(0, 5) == "tree ")
                tree_id = line.substr(5);
            else if (line.substr(0, 7) == "parent ")
                parents.push_back(line.substr(7));
            else if (line.substr(0, 7) == "author ")
            {
                // "author Name <email> <timestamp>"
                const auto last_space = line.rfind(' ');
                timestamp = (last_space != std::string::npos) ? line.substr(last_space + 1) : "";
                author    = line.substr(7, last_space - 7);
            }
        }
        else
        {
            if (!message.empty())
                message += '\n';
            message += line;
        }
    }

    // Print in a git log-like style.
    std::cout << "commit " << sha << '\n';
    std::cout << "Author: " << author << '\n';
    std::cout << "Date:   " << timestamp << '\n';
    std::cout << '\n';
    std::cout << "    " << message << '\n';
    std::cout << '\n';
}

// ---------------------------------------------------------------------------
// Public command
// ---------------------------------------------------------------------------

void log()
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(std::filesystem::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    std::string current_sha = resolve_head(repo.git_dir());

    if (current_sha.empty())
    {
        std::cerr << "fatal: your current branch has no commits yet\n";
        std::exit(1);
    }

    ObjectDatabase db(repo.git_dir() / "objects");

    // Walk the commit chain.
    while (!current_sha.empty())
    {
        std::string raw;
        try
        {
            raw = db.read(current_sha);
        }
        catch (const std::exception &e)
        {
            std::cerr << "error: " << e.what() << '\n';
            break;
        }

        print_commit(current_sha, raw);

        // Extract first parent for linear traversal.
        const auto null_pos = raw.find('\0');
        if (null_pos == std::string::npos)
            break;

        std::istringstream body(raw.substr(null_pos + 1));
        std::string line;
        current_sha.clear();

        while (std::getline(body, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                break; // end of headers
            if (line.substr(0, 7) == "parent ")
            {
                current_sha = line.substr(7);
                break;
            }
        }
    }
}
