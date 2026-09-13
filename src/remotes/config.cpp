#include "config.h"

#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace
{
    // Trim leading/trailing whitespace.
    std::string trim(std::string s)
    {
        const auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        const auto last  = s.find_last_not_of(" \t\r\n");
        return s.substr(first, last - first + 1);
    }
}

RemoteConfig::RemoteConfig(const std::filesystem::path &config_path)
    : config_path_(config_path)
{
    load();
}

void RemoteConfig::load()
{
    remotes_.clear();

    std::ifstream f(config_path_);
    if (!f) return; // empty config — fine

    // Simple state-machine parser.
    // Recognises:
    //   [remote "name"]
    //       url = value
    std::string current_remote;
    std::string line;

    const std::regex section_re(R"re(\[remote\s+"([^"]+)"\])re");

    while (std::getline(f, line))
    {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        std::smatch m;
        if (std::regex_match(trimmed, m, section_re))
        {
            current_remote = m[1].str();
            remotes_.push_back({current_remote, {}});
            continue;
        }

        // Key-value inside a section.
        if (!current_remote.empty())
        {
            const auto eq = trimmed.find('=');
            if (eq != std::string::npos)
            {
                const std::string key   = trim(trimmed.substr(0, eq));
                const std::string value = trim(trimmed.substr(eq + 1));
                if (key == "url")
                {
                    // Find the last-added entry for this remote.
                    for (auto it = remotes_.rbegin(); it != remotes_.rend(); ++it)
                    {
                        if (it->name == current_remote)
                        {
                            it->url = value;
                            break;
                        }
                    }
                }
            }
        }
    }
}

void RemoteConfig::save() const
{
    // Read non-remote lines first, then rewrite the whole file.
    std::vector<std::string> other_lines;
    {
        std::ifstream f(config_path_);
        if (f)
        {
            const std::regex remote_section_re(R"re(\[remote\s+"[^"]+"\])re");
            bool in_remote = false;
            std::string line;
            while (std::getline(f, line))
            {
                const std::string t = trim(line);
                if (std::regex_match(t, remote_section_re))
                {
                    in_remote = true;
                    continue;
                }
                if (!t.empty() && t[0] == '[')
                {
                    in_remote = false;
                }
                if (!in_remote)
                {
                    other_lines.push_back(line);
                }
            }
        }
    }

    std::ofstream f(config_path_, std::ios::trunc);
    if (!f)
        throw std::runtime_error("Cannot write config: " + config_path_.string());

    // Re-emit non-remote lines.
    for (const auto &l : other_lines)
        f << l << '\n';

    // Emit remotes.
    for (const auto &r : remotes_)
    {
        f << "[remote \"" << r.name << "\"]\n";
        f << "\turl = " << r.url << '\n';
    }
}

const RemoteEntry *RemoteConfig::find(const std::string &name) const
{
    for (const auto &r : remotes_)
        if (r.name == name) return &r;
    return nullptr;
}

void RemoteConfig::add(const std::string &name, const std::string &url)
{
    if (find(name))
        throw std::runtime_error("error: remote '" + name + "' already exists");
    remotes_.push_back({name, url});
    save();
}

void RemoteConfig::remove(const std::string &name)
{
    const auto it = std::remove_if(remotes_.begin(), remotes_.end(),
                                   [&](const RemoteEntry &r){ return r.name == name; });
    if (it == remotes_.end())
        throw std::runtime_error("error: no such remote '" + name + "'");
    remotes_.erase(it, remotes_.end());
    save();
}

const std::vector<RemoteEntry> &RemoteConfig::remotes() const
{
    return remotes_;
}
