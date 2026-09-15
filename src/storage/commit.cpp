#include "commit.h"

#include "core/sha256.h"

#include <chrono>
#include <ctime>
#include <sstream>
#include <utility>

Commit::Commit(
    const std::string &tree_id,
    const std::vector<std::string> &parent_ids,
    const std::string &author,
    const std::string &message)
    : tree_id_(tree_id),
      parent_ids_(parent_ids),
      author_(author),
      message_(message)
{
    // Capture the current Unix timestamp.
    const auto now = std::chrono::system_clock::now();
    const auto epoch =
        std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch())
            .count();
    timestamp_ = std::to_string(epoch);
}

std::string Commit::serialized() const
{
    // Build the commit body (similar to Git's commit object format).
    std::string body;
    body += "tree " + tree_id_ + '\n';

    for (const auto &pid : parent_ids_)
        body += "parent " + pid + '\n';

    body += "author "    + author_ + ' ' + timestamp_ + '\n';
    body += "committer " + author_ + ' ' + timestamp_ + '\n';
    body += '\n';
    body += message_ + '\n';

    return "commit " + std::to_string(body.size()) + '\0' + body;
}

std::string Commit::id() const
{
    return sha256(serialized());
}
