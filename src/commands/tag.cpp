#include "tag.h"

#include "../hashing/sha256.h"
#include "../objects/object_database.h"
#include "../repository/repository.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Internal helpers  (same patterns as branch.cpp / commit.cpp)
// ---------------------------------------------------------------------------

static std::string read_text(const fs::path &path)
{
    std::ifstream f(path);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string trim_trailing(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

// Resolve HEAD to a commit SHA (empty if repository has no commits yet).
static std::string resolve_head(const fs::path &git_dir)
{
    const std::string raw = trim_trailing(read_text(git_dir / "HEAD"));
    if (raw.empty()) return {};
    if (raw.substr(0, 5) == "ref: ")
        return trim_trailing(read_text(git_dir / raw.substr(5)));
    return raw; // detached HEAD
}

// ---------------------------------------------------------------------------
// tag_command — four operating modes
// ---------------------------------------------------------------------------

void tag_command(const std::string &name, bool annotated,
                 const std::string &message, bool delete_tag)
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(fs::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    const fs::path tags_dir = repo.git_dir() / "refs" / "tags";

    // -----------------------------------------------------------------------
    // MODE 1 – List all tags.
    // -----------------------------------------------------------------------
    if (name.empty())
    {
        if (!fs::exists(tags_dir))
        {
            // Directory absent → no tags (should not normally happen after init).
            return;
        }

        std::vector<std::string> tags;
        for (const auto &entry : fs::directory_iterator(tags_dir))
            if (entry.is_regular_file())
                tags.push_back(entry.path().filename().string());

        std::sort(tags.begin(), tags.end());

        for (const auto &t : tags)
            std::cout << t << '\n';

        return;
    }

    // -----------------------------------------------------------------------
    // MODE 4 – Delete a tag.
    // -----------------------------------------------------------------------
    if (delete_tag)
    {
        const fs::path ref_path = tags_dir / name;
        if (!fs::exists(ref_path))
        {
            std::cerr << "error: tag '" << name << "' not found\n";
            std::exit(1);
        }
        fs::remove(ref_path);
        std::cout << "Deleted tag '" << name << "'\n";
        return;
    }

    // -----------------------------------------------------------------------
    // Shared pre-check: tag must not already exist.
    // -----------------------------------------------------------------------
    const fs::path ref_path = tags_dir / name;
    if (fs::exists(ref_path))
    {
        std::cerr << "fatal: tag '" << name << "' already exists\n";
        std::exit(1);
    }

    // Resolve HEAD to a commit SHA.
    const std::string commit_sha = resolve_head(repo.git_dir());
    if (commit_sha.empty())
    {
        std::cerr << "fatal: not a valid object name: 'HEAD'\n";
        std::exit(1);
    }

    // -----------------------------------------------------------------------
    // MODE 2 – Lightweight tag: write commit SHA directly to refs/tags/<name>.
    // -----------------------------------------------------------------------
    if (!annotated)
    {
        // Ensure the tags directory exists (it should after init, but be safe).
        fs::create_directories(tags_dir);

        std::ofstream f(ref_path);
        if (!f)
            throw std::runtime_error("Could not create tag ref: " + ref_path.string());

        f << commit_sha << '\n';
        std::cout << "Created lightweight tag '" << name << "' at "
                  << commit_sha.substr(0, 7) << '\n';
        return;
    }

    // -----------------------------------------------------------------------
    // MODE 3 – Annotated tag: build a tag object and store it.
    // -----------------------------------------------------------------------
    if (message.empty())
    {
        std::cerr << "error: annotated tag requires a message (-m <msg>)\n";
        std::exit(1);
    }

    // Timestamp: seconds since Unix epoch.
    const auto now = std::chrono::system_clock::now();
    const long long timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();

    const std::string tagger = "MiniGit User <user@minigit>";

    // Build the tag object body (without the envelope header).
    //   object <commit-sha>\n
    //   type commit\n
    //   tag <name>\n
    //   tagger <tagger> <timestamp>\n
    //   \n
    //   <message>\n
    std::string body;
    body += "object " + commit_sha + '\n';
    body += "type commit\n";
    body += "tag " + name + '\n';
    body += "tagger " + tagger + ' ' + std::to_string(timestamp) + '\n';
    body += '\n';
    body += message + '\n';

    // Prepend the envelope: "tag <size>\0"
    const std::string header = "tag " + std::to_string(body.size()) + '\0';
    const std::string full_data = header + body;

    // Compute SHA-256 of the full data.
    const std::string tag_sha = sha256(full_data);

    // Persist the tag object.
    ObjectDatabase db(repo.git_dir() / "objects");
    db.write(tag_sha, full_data);

    // Write the tag object's SHA into refs/tags/<name>.
    fs::create_directories(tags_dir);
    std::ofstream f(ref_path);
    if (!f)
        throw std::runtime_error("Could not create tag ref: " + ref_path.string());

    f << tag_sha << '\n';
    std::cout << "Created annotated tag '" << name << "' at "
              << tag_sha.substr(0, 7) << '\n';
}
