#include "semver.h"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace minigit::update {

namespace {

std::string_view trim_sv(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return sv;
}

bool parse_uint(std::string_view sv, int& out) {
    if (sv.empty()) return false;
    int val = 0;
    for (char c : sv) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        val = val * 10 + (c - '0');
    }
    out = val;
    return true;
}

} // namespace

std::optional<SemVer> SemVer::parse(std::string_view str) {
    str = trim_sv(str);
    if (str.empty()) return std::nullopt;

    if (str.front() == 'v' || str.front() == 'V') {
        str.remove_prefix(1);
    }

    std::string prerelease;
    auto hyphen_pos = str.find('-');
    if (hyphen_pos != std::string_view::npos) {
        prerelease = std::string(str.substr(hyphen_pos + 1));
        str = str.substr(0, hyphen_pos);
    }

    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start < str.size()) {
        size_t dot_pos = str.find('.', start);
        if (dot_pos == std::string_view::npos) {
            parts.push_back(str.substr(start));
            break;
        }
        parts.push_back(str.substr(start, dot_pos - start));
        start = dot_pos + 1;
    }

    if (parts.empty() || parts.size() > 3) return std::nullopt;

    SemVer ver;
    ver.prerelease = std::move(prerelease);

    if (!parse_uint(parts[0], ver.major)) return std::nullopt;
    if (parts.size() >= 2) {
        if (!parse_uint(parts[1], ver.minor)) return std::nullopt;
    }
    if (parts.size() >= 3) {
        if (!parse_uint(parts[2], ver.patch)) return std::nullopt;
    }

    return ver;
}

bool SemVer::operator==(const SemVer& other) const {
    return major == other.major &&
           minor == other.minor &&
           patch == other.patch &&
           prerelease == other.prerelease;
}

bool SemVer::operator<(const SemVer& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;

    // Normal version has higher precedence than pre-release of same (major, minor, patch)
    if (prerelease.empty() && !other.prerelease.empty()) return false;
    if (!prerelease.empty() && other.prerelease.empty()) return true;

    return prerelease < other.prerelease;
}

std::string SemVer::to_string() const {
    std::string res = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    if (!prerelease.empty()) {
        res += "-" + prerelease;
    }
    return res;
}

} // namespace minigit::update
