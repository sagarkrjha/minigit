#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// RemoteConfig — reads and writes the [remote "name"] sections of
// .minigit/config.
//
// File format (subset of git-config INI):
//   [remote "origin"]
//       url = /path/to/repo
// ---------------------------------------------------------------------------

struct RemoteEntry
{
    std::string name;
    std::string url;
};

class RemoteConfig
{
public:
    explicit RemoteConfig(const std::filesystem::path &config_path);

    // Persist current state to disk.
    void save() const;

    // Returns nullptr if no remote with that name exists.
    const RemoteEntry *find(const std::string &name) const;

    // Add a remote; throws if name already exists.
    void add(const std::string &name, const std::string &url);

    // Remove a remote; throws if name does not exist.
    void remove(const std::string &name);

    const std::vector<RemoteEntry> &remotes() const;

private:
    std::filesystem::path config_path_;
    std::vector<RemoteEntry> remotes_;

    void load();
};
