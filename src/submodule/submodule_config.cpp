#include "submodule_config.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace
{

std::string trim(std::string s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string normalize_path(std::string p)
{
    std::replace(p.begin(), p.end(), '\\', '/');
    while (!p.empty() && p.back() == '/')
        p.pop_back();
    return p;
}

} // namespace

SubmoduleConfig::SubmoduleConfig(const std::filesystem::path &file_path)
    : file_path_(file_path)
{
    load();
}

void SubmoduleConfig::load()
{
    entries_.clear();
    std::ifstream f(file_path_);
    if (!f)
        return;

    std::string line;
    std::string current_section;
    SubmoduleEntry current_entry;
    bool in_submodule = false;

    const std::regex section_re(R"re(\[submodule\s+"([^"]+)"\])re");

    auto commit_current = [&]() {
        if (in_submodule && !current_entry.name.empty() && !current_entry.path.empty())
        {
            current_entry.path = normalize_path(current_entry.path);
            entries_.push_back(current_entry);
        }
        current_entry = SubmoduleEntry{};
        in_submodule = false;
    };

    while (std::getline(f, line))
    {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';')
            continue;

        std::smatch m;
        if (std::regex_match(trimmed, m, section_re))
        {
            commit_current();
            current_entry.name = m[1].str();
            in_submodule = true;
            continue;
        }

        if (in_submodule)
        {
            const auto eq = trimmed.find('=');
            if (eq != std::string::npos)
            {
                const std::string key = trim(trimmed.substr(0, eq));
                const std::string val = trim(trimmed.substr(eq + 1));
                if (key == "path")
                    current_entry.path = val;
                else if (key == "url")
                    current_entry.url = val;
                else if (key == "branch")
                    current_entry.branch = val;
            }
        }
    }

    commit_current();
}

void SubmoduleConfig::save() const
{
    if (entries_.empty())
    {
        std::error_code ec;
        if (std::filesystem::exists(file_path_))
            std::filesystem::remove(file_path_, ec);
        return;
    }

    std::ofstream f(file_path_, std::ios::trunc);
    if (!f)
        return;

    for (const auto &sm : entries_)
    {
        f << "[submodule \"" << sm.name << "\"]\n";
        f << "\tpath = " << normalize_path(sm.path) << "\n";
        f << "\turl = " << sm.url << "\n";
        if (!sm.branch.empty())
            f << "\tbranch = " << sm.branch << "\n";
    }
}

const SubmoduleEntry *SubmoduleConfig::find_by_path(const std::string &path) const
{
    const std::string norm = normalize_path(path);
    for (const auto &sm : entries_)
    {
        if (normalize_path(sm.path) == norm)
            return &sm;
    }
    return nullptr;
}

const SubmoduleEntry *SubmoduleConfig::find_by_name(const std::string &name) const
{
    for (const auto &sm : entries_)
    {
        if (sm.name == name)
            return &sm;
    }
    return nullptr;
}

void SubmoduleConfig::add_or_update(const SubmoduleEntry &entry)
{
    const std::string norm_path = normalize_path(entry.path);
    for (auto &sm : entries_)
    {
        if (sm.name == entry.name || normalize_path(sm.path) == norm_path)
        {
            sm = entry;
            sm.path = norm_path;
            return;
        }
    }
    SubmoduleEntry copy = entry;
    copy.path = norm_path;
    entries_.push_back(std::move(copy));
}

bool SubmoduleConfig::remove(const std::string &path_or_name)
{
    const std::string norm = normalize_path(path_or_name);
    auto it = std::find_if(entries_.begin(), entries_.end(), [&](const SubmoduleEntry &sm) {
        return sm.name == path_or_name || normalize_path(sm.path) == norm;
    });

    if (it != entries_.end())
    {
        entries_.erase(it);
        return true;
    }
    return false;
}

bool SubmoduleConfig::is_submodule_path(const std::filesystem::path &repo_root, const std::string &rel_path)
{
    const std::string norm = normalize_path(rel_path);
    if (norm.empty() || norm == ".")
        return false;

    // 1. Check .minigitmodules if it exists
    const auto modules_file = repo_root / ".minigitmodules";
    if (std::filesystem::exists(modules_file))
    {
        SubmoduleConfig cfg(modules_file);
        if (cfg.find_by_path(norm) != nullptr)
            return true;
    }

    // 2. Check filesystem for .minigit or .git indicator
    const auto abs_path = repo_root / norm;
    std::error_code ec;
    if (std::filesystem::is_directory(abs_path, ec))
    {
        if (std::filesystem::exists(abs_path / ".minigit", ec) ||
            std::filesystem::exists(abs_path / ".git", ec))
        {
            return true;
        }
    }

    return false;
}

void SubmoduleConfig::set_config_entry(const std::filesystem::path &git_dir,
                                      const std::string &name,
                                      const std::string &url,
                                      bool active)
{
    const auto config_path = git_dir / "config";
    std::vector<std::string> lines;
    std::ifstream in(config_path);
    if (in)
    {
        std::string l;
        while (std::getline(in, l))
            lines.push_back(l);
    }

    const std::regex section_re(R"re(\[submodule\s+"([^"]+)"\])re");
    int section_start = -1;
    int section_end = -1;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        std::string trimmed = trim(lines[i]);
        std::smatch m;
        if (std::regex_match(trimmed, m, section_re))
        {
            if (m[1].str() == name)
            {
                section_start = static_cast<int>(i);
                // find where this section ends
                size_t j = i + 1;
                while (j < lines.size() && !trim(lines[j]).starts_with("["))
                    j++;
                section_end = static_cast<int>(j);
                break;
            }
        }
    }

    std::vector<std::string> new_section;
    new_section.push_back("[submodule \"" + name + "\"]");
    new_section.push_back("\turl = " + url);
    if (active)
        new_section.push_back("\tactive = true");

    if (section_start != -1)
    {
        lines.erase(lines.begin() + section_start, lines.begin() + section_end);
        lines.insert(lines.begin() + section_start, new_section.begin(), new_section.end());
    }
    else
    {
        if (!lines.empty() && !trim(lines.back()).empty())
            lines.push_back("");
        lines.insert(lines.end(), new_section.begin(), new_section.end());
    }

    std::ofstream out(config_path, std::ios::trunc);
    for (const auto &l : lines)
        out << l << '\n';
}

std::optional<std::string> SubmoduleConfig::get_config_url(const std::filesystem::path &git_dir,
                                                          const std::string &name)
{
    const auto config_path = git_dir / "config";
    std::ifstream in(config_path);
    if (!in)
        return std::nullopt;

    const std::regex section_re(R"re(\[submodule\s+"([^"]+)"\])re");
    bool in_section = false;
    std::string line;

    while (std::getline(in, line))
    {
        const std::string trimmed = trim(line);
        std::smatch m;
        if (std::regex_match(trimmed, m, section_re))
        {
            in_section = (m[1].str() == name);
            continue;
        }

        if (in_section)
        {
            if (trimmed.starts_with("["))
                break;
            const auto eq = trimmed.find('=');
            if (eq != std::string::npos)
            {
                const std::string key = trim(trimmed.substr(0, eq));
                const std::string val = trim(trimmed.substr(eq + 1));
                if (key == "url")
                    return val;
            }
        }
    }

    return std::nullopt;
}

bool SubmoduleConfig::is_config_active(const std::filesystem::path &git_dir,
                                      const std::string &name)
{
    const auto config_path = git_dir / "config";
    std::ifstream in(config_path);
    if (!in)
        return false;

    const std::regex section_re(R"re(\[submodule\s+"([^"]+)"\])re");
    bool in_section = false;
    std::string line;

    while (std::getline(in, line))
    {
        const std::string trimmed = trim(line);
        std::smatch m;
        if (std::regex_match(trimmed, m, section_re))
        {
            in_section = (m[1].str() == name);
            continue;
        }

        if (in_section)
        {
            if (trimmed.starts_with("["))
                break;
            const auto eq = trimmed.find('=');
            if (eq != std::string::npos)
            {
                const std::string key = trim(trimmed.substr(0, eq));
                const std::string val = trim(trimmed.substr(eq + 1));
                if (key == "active")
                    return (val == "true" || val == "1" || val == "yes");
                if (key == "url")
                    return true; // Having a URL implicitly registers it as active
            }
        }
    }

    return false;
}

void SubmoduleConfig::remove_config_entry(const std::filesystem::path &git_dir,
                                         const std::string &name)
{
    const auto config_path = git_dir / "config";
    std::vector<std::string> lines;
    std::ifstream in(config_path);
    if (!in)
        return;

    std::string l;
    while (std::getline(in, l))
        lines.push_back(l);

    const std::regex section_re(R"re(\[submodule\s+"([^"]+)"\])re");
    int section_start = -1;
    int section_end = -1;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        std::string trimmed = trim(lines[i]);
        std::smatch m;
        if (std::regex_match(trimmed, m, section_re))
        {
            if (m[1].str() == name)
            {
                section_start = static_cast<int>(i);
                size_t j = i + 1;
                while (j < lines.size() && !trim(lines[j]).starts_with("["))
                    j++;
                section_end = static_cast<int>(j);
                break;
            }
        }
    }

    if (section_start != -1)
    {
        lines.erase(lines.begin() + section_start, lines.begin() + section_end);
        std::ofstream out(config_path, std::ios::trunc);
        for (const auto &line_out : lines)
            out << line_out << '\n';
    }
}
