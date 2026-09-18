#pragma once

#include "repository/repository.h"
#include "pkt_line.h"

#include <filesystem>
#include <string>

namespace minigit::remotes {

// Determines whether `url` specifies an HTTP/HTTPS remote protocol.
bool is_http_url(const std::string &url);

// Strips trailing slashes from HTTP URLs.
std::string normalize_url(const std::string &url);

// Discovers advertised refs for git-upload-pack (fetch/clone).
AdvertisedRefsResult discover_upload_pack(const std::string &url);

// Discovers advertised refs for git-receive-pack (push).
AdvertisedRefsResult discover_receive_pack(const std::string &url);

// Clones a remote repository over Smart HTTP/HTTPS into `dest_dir_str`.
void clone_http(const std::string &url, const std::string &dest_dir_str);

// Fetches objects and updates remote-tracking references over Smart HTTP/HTTPS.
void fetch_http(const Repository &local, const std::string &remote_name, const std::string &url);

// Pushes local branch commits and objects to a remote over Smart HTTP/HTTPS.
void push_http(const Repository &local, const std::string &remote_name, const std::string &url, const std::string &branch_name);

} // namespace minigit::remotes
