#pragma once

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Parsed representations of on-disk objects.
// All raw objects start with a header: "<type> <size>\0<body>"
// ---------------------------------------------------------------------------

struct ParsedCommit
{
    std::string tree_id;
    std::vector<std::string> parent_ids;
    std::string author;       // e.g. "MiniGit User <user@minigit>"
    std::string timestamp;    // Unix epoch string
    std::string message;
};

struct ParsedTreeEntry
{
    std::string mode;  // e.g. "100644"
    std::string name;  // filename
    std::string id;    // SHA-256 hex
};

struct ParsedTree
{
    std::vector<ParsedTreeEntry> entries;
};

// Strip the "type size\0" header and return just the body bytes.
std::string strip_object_header(const std::string &raw);

// Parse a raw commit object (as returned by ObjectDatabase::read).
ParsedCommit parse_commit(const std::string &raw);

// Parse a raw tree object (as returned by ObjectDatabase::read).
ParsedTree parse_tree(const std::string &raw);
