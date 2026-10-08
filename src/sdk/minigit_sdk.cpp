#include "minigit_sdk.h"

#include "core/sha256.h"
#include "repository/repository.h"
#include "storage/blob.h"
#include "storage/commit.h"
#include "storage/tree.h"
#include "storage/object_database.h"
#include "storage/object_parser.h"
#include "staging/index.h"
#include "staging/ignore.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>

namespace fs = std::filesystem;

namespace minigit::sdk {

namespace {

std::string trim_str(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

std::string read_file_content(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string compute_file_hash(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::string content{
        std::istreambuf_iterator<char>{f},
        std::istreambuf_iterator<char>{}
    };
    Blob blob(std::move(content));
    return blob.id();
}

std::set<std::string> scan_working_tree(const fs::path& root, const IgnoreRules& ignore_rules) {
    std::set<std::string> result;
    if (!fs::exists(root)) return result;

    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;

        const auto rel = fs::relative(entry.path(), root);
        const std::string rel_str = rel.generic_string();

        if (rel_str.starts_with(".minigit") || rel_str.starts_with(".git"))
            continue;

        if (rel_str == ".minigitignore")
            continue;

        if (ignore_rules.is_ignored(rel_str))
            continue;

        result.insert(rel_str);
    }
    return result;
}

} // namespace

class MiniGitClient::Impl {
public:
    Repository repo_;
    bool valid_{false};

    Impl() : repo_(fs::current_path()) {}

    explicit Impl(const fs::path& path) : repo_(path) {
        try {
            repo_ = Repository::discover(path);
            valid_ = true;
        } catch (...) {
            valid_ = false;
        }
    }
};

MiniGitClient::MiniGitClient() : impl_(std::make_shared<Impl>()) {}

MiniGitClient::MiniGitClient(const fs::path& repo_root) 
    : impl_(std::make_shared<Impl>(repo_root)) {}

MiniGitClient::~MiniGitClient() = default;

MiniGitClient MiniGitClient::init(const fs::path& path) {
    fs::create_directories(path);
    Repository repo(path);
    repo.init();
    return MiniGitClient(path);
}

MiniGitClient MiniGitClient::open(const fs::path& path) {
    return MiniGitClient(path);
}

bool MiniGitClient::is_valid() const {
    return impl_ && impl_->valid_;
}

fs::path MiniGitClient::root_path() const {
    if (!is_valid()) return {};
    return impl_->repo_.root();
}

fs::path MiniGitClient::git_dir_path() const {
    if (!is_valid()) return {};
    return impl_->repo_.git_dir();
}

bool MiniGitClient::add(const std::vector<std::string>& filepaths) {
    if (!is_valid()) return false;

    Index index(impl_->repo_.git_dir() / "index");
    ObjectDatabase db(impl_->repo_.git_dir() / "objects");
    const IgnoreRules ignore_rules = IgnoreRules::load(impl_->repo_.root());

    for (const auto& rel_path : filepaths) {
        fs::path full_path = impl_->repo_.root() / rel_path;
        if (!fs::exists(full_path)) {
            // Check if deleted
            if (index.has_entry(rel_path)) {
                index.remove(rel_path);
            }
            continue;
        }

        if (fs::is_directory(full_path)) {
            for (const auto& entry : fs::recursive_directory_iterator(full_path)) {
                if (!entry.is_regular_file()) continue;
                auto sub_rel = fs::relative(entry.path(), impl_->repo_.root()).generic_string();
                if (sub_rel.starts_with(".minigit") || sub_rel.starts_with(".git")) continue;
                if (ignore_rules.is_ignored(sub_rel)) continue;

                std::string content = read_file_content(entry.path());
                Blob blob(std::move(content));
                db.write(blob.id(), blob.serialized());
                index.add(sub_rel, blob.id());
            }
        } else if (fs::is_regular_file(full_path)) {
            if (ignore_rules.is_ignored(rel_path)) continue;
            std::string content = read_file_content(full_path);
            Blob blob(std::move(content));
            db.write(blob.id(), blob.serialized());
            index.add(rel_path, blob.id());
        }
    }

    index.write();
    return true;
}

bool MiniGitClient::add_all() {
    if (!is_valid()) return false;

    const IgnoreRules ignore_rules = IgnoreRules::load(impl_->repo_.root());
    const auto files = scan_working_tree(impl_->repo_.root(), ignore_rules);

    std::vector<std::string> to_add(files.begin(), files.end());
    return add(to_add);
}

std::string MiniGitClient::commit(const std::string& message, const std::string& author) {
    if (!is_valid() || message.empty()) return {};

    Index index(impl_->repo_.git_dir() / "index");
    if (index.entries().empty()) {
        return {};
    }

    std::vector<TreeEntry> tree_entries;
    tree_entries.reserve(index.entries().size());
    for (const auto& [path, blob_id] : index.entries()) {
        tree_entries.push_back({"100644", path, blob_id});
    }

    Tree tree(std::move(tree_entries));
    ObjectDatabase db(impl_->repo_.git_dir() / "objects");
    db.write(tree.id(), tree.serialized());

    std::vector<std::string> parents;
    const std::string parent_sha = Repository::resolve_head_from_dir(impl_->repo_.git_dir());
    if (!parent_sha.empty()) {
        parents.push_back(parent_sha);
    }

    const std::string author_name = author.empty() ? "MiniGit SDK <sdk@minigit>" : author;
    Commit commit_obj(tree.id(), parents, author_name, message);
    db.write(commit_obj.id(), commit_obj.serialized());

    // Update ref
    const std::string head_raw = trim_str(read_file_content(impl_->repo_.git_dir() / "HEAD"));
    fs::path ref_path;
    if (head_raw.substr(0, 5) == "ref: ") {
        ref_path = Repository::resolve_path(impl_->repo_.git_dir(), head_raw.substr(5));
        fs::create_directories(ref_path.parent_path());
    } else {
        ref_path = impl_->repo_.git_dir() / "HEAD";
    }

    std::ofstream f(ref_path, std::ios::trunc);
    if (f) {
        f << commit_obj.id() << '\n';
    }

    return commit_obj.id();
}

RepositoryStatus MiniGitClient::status() const {
    RepositoryStatus st;
    if (!is_valid()) {
        st.is_clean = false;
        return st;
    }

    st.current_branch = current_branch();
    st.head_commit_id = Repository::resolve_head_from_dir(impl_->repo_.git_dir());

    Index index(impl_->repo_.git_dir() / "index");
    const auto& staged = index.entries();

    const IgnoreRules ignore_rules = IgnoreRules::load(impl_->repo_.root());
    const auto wt_files = scan_working_tree(impl_->repo_.root(), ignore_rules);

    for (const auto& [path, blob_id] : staged) {
        fs::path abs = impl_->repo_.root() / path;
        StatusEntry entry;
        entry.path = path;
        if (!fs::exists(abs)) {
            entry.unstaged_status = "deleted";
            st.is_clean = false;
        } else {
            std::string wt_hash = compute_file_hash(abs);
            if (wt_hash != blob_id) {
                entry.unstaged_status = "modified";
                st.is_clean = false;
            }
        }
        entry.staged_status = "staged";
        st.entries.push_back(std::move(entry));
    }

    for (const auto& wt_path : wt_files) {
        if (staged.find(wt_path) == staged.end()) {
            StatusEntry entry;
            entry.path = wt_path;
            entry.unstaged_status = "untracked";
            st.entries.push_back(std::move(entry));
            st.is_clean = false;
        }
    }

    return st;
}

std::vector<std::string> MiniGitClient::list_branches() const {
    std::vector<std::string> branches;
    if (!is_valid()) return branches;

    fs::path heads_dir = impl_->repo_.common_dir() / "refs" / "heads";
    if (fs::exists(heads_dir)) {
        for (const auto& entry : fs::directory_iterator(heads_dir)) {
            if (entry.is_regular_file()) {
                branches.push_back(entry.path().filename().string());
            }
        }
        std::sort(branches.begin(), branches.end());
    }
    return branches;
}

std::string MiniGitClient::current_branch() const {
    if (!is_valid()) return {};

    const std::string raw = trim_str(read_file_content(impl_->repo_.git_dir() / "HEAD"));
    if (raw.substr(0, 5) == "ref: ") {
        const std::string ref = raw.substr(5);
        const auto slash = ref.rfind('/');
        return (slash != std::string::npos) ? ref.substr(slash + 1) : ref;
    }
    return "HEAD (detached)";
}

bool MiniGitClient::create_branch(const std::string& branch_name) {
    if (!is_valid() || branch_name.empty()) return false;

    fs::path heads_dir = impl_->repo_.common_dir() / "refs" / "heads";
    fs::path ref_path = heads_dir / branch_name;
    if (fs::exists(ref_path)) return false;

    std::string head_sha = Repository::resolve_head_from_dir(impl_->repo_.git_dir());
    if (head_sha.empty()) return false;

    fs::create_directories(ref_path.parent_path());
    std::ofstream f(ref_path);
    if (!f) return false;
    f << head_sha << '\n';
    return true;
}

bool MiniGitClient::switch_branch(const std::string& branch_name) {
    if (!is_valid() || branch_name.empty()) return false;

    fs::path ref_path = impl_->repo_.common_dir() / "refs" / "heads" / branch_name;
    if (!fs::exists(ref_path)) return false;

    std::ofstream head_file(impl_->repo_.git_dir() / "HEAD");
    if (!head_file) return false;
    head_file << "ref: refs/heads/" << branch_name << '\n';
    return true;
}

std::vector<CommitInfo> MiniGitClient::log(size_t max_count) const {
    std::vector<CommitInfo> history;
    if (!is_valid()) return history;

    std::string current_sha = Repository::resolve_head_from_dir(impl_->repo_.git_dir());
    ObjectDatabase db(impl_->repo_.git_dir() / "objects");

    while (!current_sha.empty() && history.size() < max_count) {
        try {
            std::string raw = db.read(current_sha);
            ParsedCommit parsed = parse_commit(raw);

            CommitInfo info;
            info.commit_id = current_sha;
            info.tree_id = parsed.tree_id;
            info.parent_ids = parsed.parent_ids;
            info.author = parsed.author;
            info.timestamp = parsed.timestamp;
            info.message = parsed.message;
            history.push_back(std::move(info));

            current_sha = parsed.parent_ids.empty() ? "" : parsed.parent_ids[0];
        } catch (...) {
            break;
        }
    }
    return history;
}

std::optional<CommitInfo> MiniGitClient::get_commit(const std::string& commit_id) const {
    if (!is_valid() || commit_id.empty()) return std::nullopt;

    try {
        ObjectDatabase db(impl_->repo_.git_dir() / "objects");
        std::string raw = db.read(commit_id);
        ParsedCommit parsed = parse_commit(raw);

        CommitInfo info;
        info.commit_id = commit_id;
        info.tree_id = parsed.tree_id;
        info.parent_ids = parsed.parent_ids;
        info.author = parsed.author;
        info.timestamp = parsed.timestamp;
        info.message = parsed.message;
        return info;
    } catch (...) {
        return std::nullopt;
    }
}

std::string MiniGitClient::get_object_content(const std::string& object_id) const {
    if (!is_valid() || object_id.empty()) return {};

    try {
        ObjectDatabase db(impl_->repo_.git_dir() / "objects");
        std::string raw = db.read(object_id);
        return strip_object_header(raw);
    } catch (...) {
        return {};
    }
}

bool MiniGitClient::sync_to_cloud(const std::string& bucket_or_local_target, const std::string& prefix) {
    if (!is_valid()) return false;

    // Supports local directory emulation of cloud storage bucket/folder
    fs::path target_base = bucket_or_local_target;
    if (!prefix.empty()) {
        target_base /= prefix;
    }

    try {
        fs::path backup_objects = target_base / "objects";
        fs::path backup_refs = target_base / "refs";
        fs::create_directories(backup_objects);
        fs::create_directories(backup_refs);

        fs::path repo_objects = impl_->repo_.objects_dir();
        if (fs::exists(repo_objects)) {
            for (const auto& entry : fs::recursive_directory_iterator(repo_objects)) {
                if (entry.is_regular_file()) {
                    auto rel = fs::relative(entry.path(), repo_objects);
                    auto dst = backup_objects / rel;
                    fs::create_directories(dst.parent_path());
                    fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing);
                }
            }
        }

        fs::path repo_refs = impl_->repo_.refs_dir();
        if (fs::exists(repo_refs)) {
            for (const auto& entry : fs::recursive_directory_iterator(repo_refs)) {
                if (entry.is_regular_file()) {
                    auto rel = fs::relative(entry.path(), repo_refs);
                    auto dst = backup_refs / rel;
                    fs::create_directories(dst.parent_path());
                    fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing);
                }
            }
        }

        fs::path head_file = impl_->repo_.head_path();
        if (fs::exists(head_file)) {
            fs::copy_file(head_file, target_base / "HEAD", fs::copy_options::overwrite_existing);
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool MiniGitClient::restore_from_cloud(const std::string& bucket_or_local_source, const std::string& prefix) {
    if (!is_valid()) return false;

    fs::path source_base = bucket_or_local_source;
    if (!prefix.empty()) {
        source_base /= prefix;
    }

    if (!fs::exists(source_base)) return false;

    try {
        fs::path src_objects = source_base / "objects";
        if (fs::exists(src_objects)) {
            for (const auto& entry : fs::recursive_directory_iterator(src_objects)) {
                if (entry.is_regular_file()) {
                    auto rel = fs::relative(entry.path(), src_objects);
                    auto dst = impl_->repo_.objects_dir() / rel;
                    fs::create_directories(dst.parent_path());
                    fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing);
                }
            }
        }

        fs::path src_refs = source_base / "refs";
        if (fs::exists(src_refs)) {
            for (const auto& entry : fs::recursive_directory_iterator(src_refs)) {
                if (entry.is_regular_file()) {
                    auto rel = fs::relative(entry.path(), src_refs);
                    auto dst = impl_->repo_.refs_dir() / rel;
                    fs::create_directories(dst.parent_path());
                    fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing);
                }
            }
        }

        fs::path src_head = source_base / "HEAD";
        if (fs::exists(src_head)) {
            fs::copy_file(src_head, impl_->repo_.head_path(), fs::copy_options::overwrite_existing);
        }

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace minigit::sdk

/* ========================================================================= */
/* C API implementation */
/* ========================================================================= */

static char* duplicate_string(const std::string& str) {
    char* copy = static_cast<char*>(std::malloc(str.size() + 1));
    if (copy) {
        std::memcpy(copy, str.c_str(), str.size() + 1);
    }
    return copy;
}

extern "C" {

minigit_client_handle_t minigit_sdk_init(const char* path) {
    if (!path) return nullptr;
    auto* client = new minigit::sdk::MiniGitClient(minigit::sdk::MiniGitClient::init(path));
    return static_cast<minigit_client_handle_t>(client);
}

minigit_client_handle_t minigit_sdk_open(const char* path) {
    if (!path) return nullptr;
    auto* client = new minigit::sdk::MiniGitClient(minigit::sdk::MiniGitClient::open(path));
    return static_cast<minigit_client_handle_t>(client);
}

void minigit_sdk_free(minigit_client_handle_t handle) {
    delete static_cast<minigit::sdk::MiniGitClient*>(handle);
}

int minigit_sdk_add(minigit_client_handle_t handle, const char* filepath) {
    if (!handle || !filepath) return 0;
    auto* client = static_cast<minigit::sdk::MiniGitClient*>(handle);
    return client->add({filepath}) ? 1 : 0;
}

int minigit_sdk_add_all(minigit_client_handle_t handle) {
    if (!handle) return 0;
    auto* client = static_cast<minigit::sdk::MiniGitClient*>(handle);
    return client->add_all() ? 1 : 0;
}

char* minigit_sdk_commit(minigit_client_handle_t handle, const char* message, const char* author) {
    if (!handle || !message) return nullptr;
    auto* client = static_cast<minigit::sdk::MiniGitClient*>(handle);
    std::string cid = client->commit(message, author ? author : "");
    return duplicate_string(cid);
}

char* minigit_sdk_get_current_branch(minigit_client_handle_t handle) {
    if (!handle) return nullptr;
    auto* client = static_cast<minigit::sdk::MiniGitClient*>(handle);
    return duplicate_string(client->current_branch());
}

int minigit_sdk_create_branch(minigit_client_handle_t handle, const char* branch_name) {
    if (!handle || !branch_name) return 0;
    auto* client = static_cast<minigit::sdk::MiniGitClient*>(handle);
    return client->create_branch(branch_name) ? 1 : 0;
}

int minigit_sdk_switch_branch(minigit_client_handle_t handle, const char* branch_name) {
    if (!handle || !branch_name) return 0;
    auto* client = static_cast<minigit::sdk::MiniGitClient*>(handle);
    return client->switch_branch(branch_name) ? 1 : 0;
}

int minigit_sdk_sync_to_cloud(minigit_client_handle_t handle, const char* target, const char* prefix) {
    if (!handle || !target) return 0;
    auto* client = static_cast<minigit::sdk::MiniGitClient*>(handle);
    return client->sync_to_cloud(target, prefix ? prefix : "") ? 1 : 0;
}

int minigit_sdk_restore_from_cloud(minigit_client_handle_t handle, const char* source, const char* prefix) {
    if (!handle || !source) return 0;
    auto* client = static_cast<minigit::sdk::MiniGitClient*>(handle);
    return client->restore_from_cloud(source, prefix ? prefix : "") ? 1 : 0;
}

void minigit_sdk_string_free(char* str) {
    std::free(str);
}

}
