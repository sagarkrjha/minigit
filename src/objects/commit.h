#pragma once

#include <string>
#include <vector>

// A Commit object references a tree, zero or more parent commits,
// and carries author/committer metadata and a message.
class Commit
{
public:
    Commit(
        const std::string &tree_id,
        const std::vector<std::string> &parent_ids,
        const std::string &author,
        const std::string &message
    );

    // Serialize to the on-disk object format.
    std::string serialized() const;

    // SHA-256 of serialized().
    std::string id() const;

private:
    std::string tree_id_;
    std::vector<std::string> parent_ids_;
    std::string author_;
    std::string message_;
    std::string timestamp_;   // filled in constructor from system clock
};
