#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace minigit::update {

struct SemVer {
    int major{0};
    int minor{0};
    int patch{0};
    std::string prerelease;

    static std::optional<SemVer> parse(std::string_view str);

    bool operator==(const SemVer& other) const;
    bool operator!=(const SemVer& other) const { return !(*this == other); }
    bool operator<(const SemVer& other) const;
    bool operator>(const SemVer& other) const { return other < *this; }
    bool operator<=(const SemVer& other) const { return !(other < *this); }
    bool operator>=(const SemVer& other) const { return !(*this < other); }

    std::string to_string() const;
};

} // namespace minigit::update
