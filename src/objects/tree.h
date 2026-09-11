#pragma once

#include <string>
#include <vector>

// A single entry in a Tree: a named reference to a blob or sub-tree.
struct TreeEntry
{
    std::string mode;    // "100644" for regular files, "040000" for dirs
    std::string name;    // filename or directory name (not a path)
    std::string id;      // SHA-256 hex of the referenced object
};

// A Tree object represents the state of a directory at a point in time.
// It holds an ordered list of TreeEntry items.
class Tree
{
public:
    explicit Tree(std::vector<TreeEntry> entries);

    const std::vector<TreeEntry>& entries() const;

    // Serialise to the on-disk format:
    //   "<mode> <name> <sha256>\n"  (one line per entry, sorted by name)
    std::string serialized() const;

    // SHA-256 of serialized().
    std::string id() const;

private:
    std::vector<TreeEntry> entries_;
};
