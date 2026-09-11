#include "tree.h"

#include "../hashing/sha256.h"

#include <algorithm>
#include <sstream>
#include <utility>

Tree::Tree(std::vector<TreeEntry> entries)
    : entries_(std::move(entries))
{
    // Keep entries sorted by name so the same directory always produces
    // the same tree ID regardless of insertion order.
    std::sort(entries_.begin(), entries_.end(),
              [](const TreeEntry &a, const TreeEntry &b)
              { return a.name < b.name; });
}

const std::vector<TreeEntry> &Tree::entries() const
{
    return entries_;
}

std::string Tree::serialized() const
{
    // Format: "tree <size>\0<entries>"
    // where each entry line is "<mode> <name> <sha256>\n"
    std::string body;
    for (const auto &e : entries_)
        body += e.mode + ' ' + e.name + ' ' + e.id + '\n';

    return "tree " + std::to_string(body.size()) + '\0' + body;
}

std::string Tree::id() const
{
    return sha256(serialized());
}
