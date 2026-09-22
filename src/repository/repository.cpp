#include "repository.h"
#include "core/logger.h"
#include <fstream>
#include <stdexcept>

Repository::Repository(const std::filesystem::path &root)
    : root_(root),
      git_dir_(root / ".minigit"),
      common_dir_(root / ".minigit"),
      is_worktree_(false)
{
    if (std::filesystem::is_regular_file(git_dir_))
    {
        std::ifstream f(git_dir_);
        std::string line;
        if (std::getline(f, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
                line.pop_back();
            size_t start_idx = 0;
            while (start_idx < line.size() && line[start_idx] == ' ')
                start_idx++;
            line = line.substr(start_idx);

            if (line.rfind("gitdir:", 0) == 0)
            {
                std::string target = line.substr(7);
                while (!target.empty() && (target.front() == ' ' || target.front() == '\t'))
                    target.erase(0, 1);
                while (!target.empty() && (target.back() == ' ' || target.back() == '\t' || target.back() == '\r'))
                    target.pop_back();

                std::filesystem::path gdir(target);
                if (gdir.is_relative())
                    git_dir_ = std::filesystem::weakly_canonical(root / gdir);
                else
                    git_dir_ = std::filesystem::weakly_canonical(gdir);

                common_dir_ = git_dir_;
                const auto commondir_file = git_dir_ / "commondir";
                if (std::filesystem::exists(commondir_file))
                {
                    std::ifstream cf(commondir_file);
                    std::string cline;
                    if (std::getline(cf, cline))
                    {
                        while (!cline.empty() && (cline.back() == '\r' || cline.back() == '\n' || cline.back() == ' '))
                            cline.pop_back();
                        size_t cstart = 0;
                        while (cstart < cline.size() && cline[cstart] == ' ')
                            cstart++;
                        cline = cline.substr(cstart);

                        std::filesystem::path cpath(cline);
                        if (cpath.is_relative())
                            common_dir_ = std::filesystem::weakly_canonical(git_dir_ / cpath);
                        else
                            common_dir_ = std::filesystem::weakly_canonical(cpath);
                    }
                }
                is_worktree_ = true;
                worktree_name_ = git_dir_.filename().string();
            }
        }
    }
}

Repository::Repository(const std::filesystem::path &root,
                       const std::filesystem::path &git_dir,
                       const std::filesystem::path &common_dir,
                       bool is_worktree,
                       const std::string &worktree_name)
    : root_(root),
      git_dir_(git_dir),
      common_dir_(common_dir),
      is_worktree_(is_worktree),
      worktree_name_(worktree_name)
{
}

Repository Repository::discover(const std::filesystem::path &start)
{
    LOG_TRACE("repository", "discovering repository starting from: " << start.string());
    auto current = std::filesystem::weakly_canonical(start);

    while (true)
    {
        const auto candidate = current / ".minigit";
        if (std::filesystem::exists(candidate))
        {
            LOG_DEBUG("repository", "discovered repository root at: " << current.string());
            if (std::filesystem::is_directory(candidate))
            {
                return Repository(current, candidate, candidate, false, "");
            }

            if (std::filesystem::is_regular_file(candidate))
            {
                std::ifstream f(candidate);
                std::string line;
                if (std::getline(f, line))
                {
                    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
                        line.pop_back();
                    size_t start_idx = 0;
                    while (start_idx < line.size() && line[start_idx] == ' ')
                        start_idx++;
                    line = line.substr(start_idx);

                    if (line.rfind("gitdir:", 0) == 0)
                    {
                        std::string target = line.substr(7);
                        while (!target.empty() && (target.front() == ' ' || target.front() == '\t'))
                            target.erase(0, 1);
                        while (!target.empty() && (target.back() == ' ' || target.back() == '\t' || target.back() == '\r'))
                            target.pop_back();

                        std::filesystem::path gdir(target);
                        if (gdir.is_relative())
                            gdir = std::filesystem::weakly_canonical(current / gdir);
                        else
                            gdir = std::filesystem::weakly_canonical(gdir);

                        std::filesystem::path common_dir = gdir;
                        const auto commondir_file = gdir / "commondir";
                        if (std::filesystem::exists(commondir_file))
                        {
                            std::ifstream cf(commondir_file);
                            std::string cline;
                            if (std::getline(cf, cline))
                            {
                                while (!cline.empty() && (cline.back() == '\r' || cline.back() == '\n' || cline.back() == ' '))
                                    cline.pop_back();
                                size_t cstart = 0;
                                while (cstart < cline.size() && cline[cstart] == ' ')
                                    cstart++;
                                cline = cline.substr(cstart);

                                std::filesystem::path cpath(cline);
                                if (cpath.is_relative())
                                    common_dir = std::filesystem::weakly_canonical(gdir / cpath);
                                else
                                    common_dir = std::filesystem::weakly_canonical(cpath);
                            }
                        }

                        std::string wt_name = gdir.filename().string();
                        return Repository(current, gdir, common_dir, true, wt_name);
                    }
                }
            }
        }

        const auto parent = current.parent_path();
        if (parent == current)
        {
            throw std::runtime_error(
                "fatal: not a minigit repository (or any of the parent "
                "directories): .minigit");
        }

        current = parent;
    }
}

const std::filesystem::path &Repository::root() const
{
    return root_;
}

const std::filesystem::path &Repository::git_dir() const
{
    return git_dir_;
}

const std::filesystem::path &Repository::common_dir() const
{
    return common_dir_;
}

bool Repository::is_worktree() const
{
    return is_worktree_;
}

const std::string &Repository::worktree_name() const
{
    return worktree_name_;
}

std::filesystem::path Repository::objects_dir() const
{
    return common_dir_ / "objects";
}

std::filesystem::path Repository::refs_dir() const
{
    return common_dir_ / "refs";
}

std::filesystem::path Repository::index_path() const
{
    return git_dir_ / "index";
}

std::filesystem::path Repository::head_path() const
{
    return git_dir_ / "HEAD";
}

std::filesystem::path Repository::resolve_path(const std::filesystem::path &git_dir,
                                              const std::filesystem::path &subpath)
{
    const auto direct = git_dir / subpath;
    if (std::filesystem::exists(direct))
        return direct;

    const auto commondir_file = git_dir / "commondir";
    if (std::filesystem::exists(commondir_file))
    {
        std::ifstream cf(commondir_file);
        std::string cline;
        if (std::getline(cf, cline))
        {
            while (!cline.empty() && (cline.back() == '\r' || cline.back() == '\n' || cline.back() == ' '))
                cline.pop_back();
            size_t cstart = 0;
            while (cstart < cline.size() && cline[cstart] == ' ')
                cstart++;
            cline = cline.substr(cstart);

            std::filesystem::path cpath(cline);
            std::filesystem::path common_dir = cpath.is_relative()
                ? std::filesystem::weakly_canonical(git_dir / cpath)
                : std::filesystem::weakly_canonical(cpath);

            if (std::filesystem::exists(common_dir / subpath))
                return common_dir / subpath;

            const std::string sub_str = subpath.generic_string();
            if (sub_str.starts_with("refs/") || sub_str == "refs" ||
                sub_str.starts_with("objects/") || sub_str == "objects" ||
                sub_str == "config" || sub_str.starts_with("worktrees"))
            {
                return common_dir / subpath;
            }
        }
    }

    return direct;
}

std::filesystem::path Repository::resolve_git_path(const std::filesystem::path &subpath) const
{
    return resolve_path(git_dir_, subpath);
}

std::string Repository::resolve_head_from_dir(const std::filesystem::path &git_dir)
{
    const auto head_file = git_dir / "HEAD";
    if (!std::filesystem::exists(head_file))
        return {};

    std::ifstream hf(head_file);
    std::string line;
    if (!std::getline(hf, line))
        return {};

    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
        line.pop_back();

    if (line.rfind("ref: ", 0) == 0)
    {
        const std::string ref_name = line.substr(5);
        const auto ref_file = resolve_path(git_dir, ref_name);
        if (std::filesystem::exists(ref_file))
        {
            std::ifstream rf(ref_file);
            std::string sha;
            if (std::getline(rf, sha))
            {
                while (!sha.empty() && (sha.back() == '\r' || sha.back() == '\n' || sha.back() == ' '))
                    sha.pop_back();
                return sha;
            }
        }
        return {};
    }

    return line;
}

Repository::BranchWorktreeMatch Repository::find_branch_worktree(const std::string &branch_name) const
{
    BranchWorktreeMatch match;

    std::string short_name = branch_name;
    if (short_name.rfind("refs/heads/", 0) == 0)
        short_name = short_name.substr(11);

    // 1. Check main worktree
    const auto main_root = common_dir_.parent_path();
    const auto main_head = common_dir_ / "HEAD";
    if (std::filesystem::exists(main_head))
    {
        std::ifstream f(main_head);
        std::string line;
        if (std::getline(f, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
                line.pop_back();
            if (line.rfind("ref: refs/heads/", 0) == 0)
            {
                std::string b = line.substr(16);
                if (b == short_name)
                {
                    match.is_checked_out = true;
                    match.worktree_path = main_root;
                    std::error_code ec;
                    match.is_current_worktree = std::filesystem::equivalent(root_, main_root, ec);
                    return match;
                }
            }
        }
    }

    // 2. Check linked worktrees in common_dir / "worktrees"
    const auto wts_dir = common_dir_ / "worktrees";
    if (std::filesystem::exists(wts_dir))
    {
        std::error_code it_ec;
        for (const auto &entry : std::filesystem::directory_iterator(wts_dir, it_ec))
        {
            if (!entry.is_directory())
                continue;

            const auto head_file = entry.path() / "HEAD";
            const auto gitdir_file = entry.path() / "gitdir";
            if (!std::filesystem::exists(head_file) || !std::filesystem::exists(gitdir_file))
                continue;

            std::ifstream hf(head_file);
            std::string hline;
            if (!std::getline(hf, hline))
                continue;
            while (!hline.empty() && (hline.back() == '\r' || hline.back() == '\n' || hline.back() == ' '))
                hline.pop_back();

            if (hline.rfind("ref: refs/heads/", 0) == 0)
            {
                std::string b = hline.substr(16);
                if (b == short_name)
                {
                    std::ifstream gf(gitdir_file);
                    std::string gline;
                    if (std::getline(gf, gline))
                    {
                        while (!gline.empty() && (gline.back() == '\r' || gline.back() == '\n' || gline.back() == ' '))
                            gline.pop_back();
                        std::filesystem::path gp(gline);
                        std::filesystem::path wt_root = gp.parent_path();
                        match.is_checked_out = true;
                        match.worktree_path = wt_root;
                        std::error_code ec;
                        match.is_current_worktree = std::filesystem::equivalent(root_, wt_root, ec);
                        return match;
                    }
                }
            }
        }
    }

    return match;
}

void Repository::init()
{
    std::filesystem::create_directories(git_dir_ / "objects");
    std::filesystem::create_directories(git_dir_ / "refs" / "heads");
    std::filesystem::create_directories(git_dir_ / "refs" / "tags");

    if (!std::filesystem::exists(git_dir_ / "HEAD"))
    {
        std::ofstream head(git_dir_ / "HEAD");

        if (!head)
        {
            throw std::runtime_error("Could not create HEAD");
        }

        head << "ref: refs/heads/main\n";
    }
    if (!std::filesystem::exists(git_dir_ / "config"))
    {
        std::ofstream config(git_dir_ / "config");

        if (!config)
        {
            throw std::runtime_error("Could not create config");
        }
    }
    if (!std::filesystem::exists(git_dir_ / "index"))
    {
        std::ofstream index(git_dir_ / "index");

        if (!index)
        {
            throw std::runtime_error("Could not create index");
        }
    }
}