#include "cat_file.h"

#include "../objects/object_database.h"
#include "../objects/object_parser.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Return the type token from the raw object header ("blob", "tree", "commit").
static std::string object_type(const std::string& raw)
{
    const auto space_pos = raw.find(' ');
    if (space_pos == std::string::npos)
        throw std::runtime_error("malformed object: missing space in header");
    return raw.substr(0, space_pos);
}

// Return the body size (the number after the space, before the null byte).
static std::size_t object_size(const std::string& raw)
{
    const auto space_pos = raw.find(' ');
    const auto null_pos  = raw.find('\0');
    if (space_pos == std::string::npos || null_pos == std::string::npos ||
        null_pos <= space_pos)
        throw std::runtime_error("malformed object: bad header format");

    return std::stoul(raw.substr(space_pos + 1, null_pos - space_pos - 1));
}

// Pretty-print a blob: just dump its raw content.
static void print_blob(const std::string& raw)
{
    const std::string body = strip_object_header(raw);
    std::cout << body;
}

// Pretty-print a tree: one entry per line in the format real git uses.
//   <mode> <type> <sha>    <name>
static void print_tree(const std::string& raw)
{
    const ParsedTree tree = parse_tree(raw);
    for (const auto& entry : tree.entries)
    {
        const std::string obj_type =
            (entry.mode == "040000") ? "tree" : "blob";

        std::cout << entry.mode << ' '
                  << obj_type   << ' '
                  << entry.id   << "    "
                  << entry.name << '\n';
    }
}

// Pretty-print a commit: header fields first, then the message.
static void print_commit(const std::string& raw)
{
    const ParsedCommit c = parse_commit(raw);

    std::cout << "tree "   << c.tree_id << '\n';
    for (const auto& p : c.parent_ids)
        std::cout << "parent " << p << '\n';
    std::cout << "author "    << c.author    << ' ' << c.timestamp << '\n';
    std::cout << "committer " << c.author    << ' ' << c.timestamp << '\n';
    std::cout << '\n';
    std::cout << c.message << '\n';
}

// Pretty-print an annotated tag: re-emit the body headers as-is.
// Tag body format:
//   object <commit-sha>
//   type commit
//   tag <name>
//   tagger <tagger> <timestamp>
//
//   <message>
static void print_tag(const std::string& raw)
{
    const std::string body = strip_object_header(raw);
    std::cout << body;
}

// ---------------------------------------------------------------------------
// cat_file
// ---------------------------------------------------------------------------

void cat_file(const std::string& mode, const std::string& object_id)
{
    ObjectDatabase db(
        std::filesystem::current_path() / ".minigit" / "objects");

    std::string raw;
    try
    {
        raw = db.read(object_id);
    }
    catch (const std::exception& e)
    {
        std::cerr << "fatal: " << e.what() << '\n';
        return;
    }

    if (mode == "-t")
    {
        std::cout << object_type(raw) << '\n';
    }
    else if (mode == "-s")
    {
        std::cout << object_size(raw) << '\n';
    }
    else if (mode == "-p")
    {
        const std::string type = object_type(raw);

        if (type == "blob")
            print_blob(raw);
        else if (type == "tree")
            print_tree(raw);
        else if (type == "commit")
            print_commit(raw);
        else if (type == "tag")
            print_tag(raw);
        else
            std::cerr << "cat-file: unknown object type '" << type << "'\n";
    }
    else
    {
        std::cerr << "usage: minigit cat-file (-t | -s | -p) <object>\n";
    }
}
