#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace minigit::remotes {

// ---------------------------------------------------------------------------
// Git Packet-Line (pkt-line) framing
// ---------------------------------------------------------------------------

// Format payload as a pkt-line string (4-hex-digit length prefix + payload)
std::string pkt_line(const std::string &payload);

// Flush packet ("0000")
std::string pkt_flush();

// Delimiter packet ("0001")
std::string pkt_delim();

struct PktLineResult
{
    std::string payload;
    bool is_flush{false};
    bool is_delim{false};
    bool is_eof{false};
};

// Reads a single pkt-line from `buffer` starting at `offset`.
// Updates `offset` to point to next pkt-line.
PktLineResult read_pkt_line(const std::string &buffer, size_t &offset);

// ---------------------------------------------------------------------------
// Advertised Refs & Capabilities Parser
// ---------------------------------------------------------------------------

struct AdvertisedRef
{
    std::string name; // e.g. "HEAD", "refs/heads/main", "refs/tags/v1.0"
    std::string sha;  // 64-character hex SHA-256
};

struct AdvertisedRefsResult
{
    std::string service;
    std::vector<AdvertisedRef> refs;
    std::unordered_map<std::string, std::string> ref_map; // name -> sha
    std::vector<std::string> capabilities;
    std::string symref_head; // Target of HEAD, e.g. "refs/heads/main"
    bool success{false};
    std::string error;
};

// Parses the response body of GET /info/refs?service=git-upload-pack or git-receive-pack
AdvertisedRefsResult parse_advertised_refs(
    const std::string &response_body,
    const std::string &expected_service
);

} // namespace minigit::remotes
