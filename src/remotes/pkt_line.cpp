#include "pkt_line.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace minigit::remotes {

std::string pkt_line(const std::string &payload)
{
    size_t total_len = payload.size() + 4;
    std::ostringstream oss;
    oss << std::hex << std::setw(4) << std::setfill('0') << total_len;
    oss << payload;
    return oss.str();
}

std::string pkt_flush()
{
    return "0000";
}

std::string pkt_delim()
{
    return "0001";
}

PktLineResult read_pkt_line(const std::string &buffer, size_t &offset)
{
    PktLineResult res;
    if (offset >= buffer.size())
    {
        res.is_eof = true;
        return res;
    }

    if (offset + 4 > buffer.size())
    {
        res.is_eof = true;
        offset = buffer.size();
        return res;
    }

    std::string len_str = buffer.substr(offset, 4);
    offset += 4;

    size_t length = 0;
    try
    {
        length = std::stoul(len_str, nullptr, 16);
    }
    catch (...)
    {
        res.is_eof = true;
        return res;
    }

    if (length == 0)
    {
        res.is_flush = true;
        return res;
    }
    if (length == 1)
    {
        res.is_delim = true;
        return res;
    }
    if (length < 4)
    {
        // Malformed
        res.is_eof = true;
        return res;
    }

    size_t payload_len = length - 4;
    if (offset + payload_len > buffer.size())
    {
        payload_len = buffer.size() - offset;
    }

    res.payload = buffer.substr(offset, payload_len);
    offset += payload_len;
    return res;
}

AdvertisedRefsResult parse_advertised_refs(
    const std::string &response_body,
    const std::string &expected_service)
{
    AdvertisedRefsResult result;
    size_t offset = 0;

    // 1. Read first line: must be service header
    PktLineResult first = read_pkt_line(response_body, offset);
    if (first.is_eof || first.payload.empty())
    {
        result.error = "Empty or truncated response from server";
        return result;
    }

    std::string expected_prefix = "# service=" + expected_service;
    std::string first_payload = first.payload;
    // Strip trailing newline
    while (!first_payload.empty() && (first_payload.back() == '\n' || first_payload.back() == '\r'))
        first_payload.pop_back();

    if (first_payload != expected_prefix)
    {
        result.error = "Expected service header '" + expected_prefix + "', got '" + first_payload + "'";
        return result;
    }
    result.service = expected_service;

    // 2. Next packet is flush "0000"
    PktLineResult flush_pkt = read_pkt_line(response_body, offset);
    if (!flush_pkt.is_flush)
    {
        // In some servers, flush may be skipped or replaced
        if (!flush_pkt.is_eof && !flush_pkt.payload.empty())
        {
            // Reset offset back if it wasn't a flush
            offset -= (flush_pkt.payload.size() + 4);
        }
    }

    // 3. Read advertised refs
    bool first_ref = true;
    while (offset < response_body.size())
    {
        PktLineResult line = read_pkt_line(response_body, offset);
        if (line.is_eof || line.is_flush)
            break;

        std::string text = line.payload;
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
            text.pop_back();

        if (text.empty())
            continue;

        if (first_ref)
        {
            first_ref = false;
            // First line format: <sha> <name>\0<capabilities>
            size_t null_pos = text.find('\0');
            std::string cap_str;
            std::string ref_str = text;
            if (null_pos != std::string::npos)
            {
                ref_str = text.substr(0, null_pos);
                cap_str = text.substr(null_pos + 1);
            }

            // Parse capabilities separated by spaces
            std::istringstream cap_iss(cap_str);
            std::string cap;
            while (cap_iss >> cap)
            {
                result.capabilities.push_back(cap);
                // Check for symref=HEAD:refs/heads/...
                if (cap.rfind("symref=HEAD:", 0) == 0)
                {
                    result.symref_head = cap.substr(12);
                }
            }

            // Parse ref
            size_t sp = ref_str.find(' ');
            if (sp != std::string::npos)
            {
                std::string sha = ref_str.substr(0, sp);
                std::string name = ref_str.substr(sp + 1);
                if (name != "capabilities^{}")
                {
                    result.refs.push_back({name, sha});
                    result.ref_map[name] = sha;
                }
            }
        }
        else
        {
            size_t sp = text.find(' ');
            if (sp != std::string::npos)
            {
                std::string sha = text.substr(0, sp);
                std::string name = text.substr(sp + 1);
                result.refs.push_back({name, sha});
                result.ref_map[name] = sha;
            }
        }
    }

    result.success = true;
    return result;
}

} // namespace minigit::remotes
