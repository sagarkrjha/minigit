#include "object_parser.h"

#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// strip_object_header
// ---------------------------------------------------------------------------

std::string strip_object_header(const std::string &raw)
{
    const auto null_pos = raw.find('\0');
    if (null_pos == std::string::npos)
        throw std::runtime_error("malformed object: no null byte in header");
    return raw.substr(null_pos + 1);
}

// ---------------------------------------------------------------------------
// parse_commit
// ---------------------------------------------------------------------------

ParsedCommit parse_commit(const std::string &raw)
{
    const std::string body = strip_object_header(raw);
    std::istringstream stream(body);
    std::string line;
    ParsedCommit result;
    bool in_message = false;

    while (std::getline(stream, line))
    {
        // Strip trailing \r (Windows line endings inside the object body).
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (in_message)
        {
            if (!result.message.empty())
                result.message += '\n';
            result.message += line;
            continue;
        }

        if (line.empty())
        {
            in_message = true;
            continue;
        }

        if (line.substr(0, 5) == "tree ")
        {
            result.tree_id = line.substr(5);
        }
        else if (line.substr(0, 7) == "parent ")
        {
            result.parent_ids.push_back(line.substr(7));
        }
        else if (line.substr(0, 7) == "author ")
        {
            // Format: "author <name> <timestamp>"
            const auto last_space = line.rfind(' ');
            if (last_space != std::string::npos)
            {
                result.timestamp = line.substr(last_space + 1);
                result.author    = line.substr(7, last_space - 7);
            }
        }
        // "committer" line is intentionally ignored (same as author for now).
    }

    // Remove trailing newline from message.
    while (!result.message.empty() && result.message.back() == '\n')
        result.message.pop_back();

    return result;
}

// ---------------------------------------------------------------------------
// parse_tree
// ---------------------------------------------------------------------------

ParsedTree parse_tree(const std::string &raw)
{
    const std::string body = strip_object_header(raw);
    std::istringstream stream(body);
    std::string line;
    ParsedTree result;

    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        // Format: "<mode> <name> <sha256>"
        const auto first_space  = line.find(' ');
        const auto second_space = line.rfind(' ');

        if (first_space == std::string::npos || first_space == second_space)
            throw std::runtime_error("malformed tree entry: \"" + line + "\"");

        ParsedTreeEntry entry;
        entry.mode = line.substr(0, first_space);
        entry.name = line.substr(first_space + 1, second_space - first_space - 1);
        entry.id   = line.substr(second_space + 1);
        result.entries.push_back(std::move(entry));
    }

    return result;
}
