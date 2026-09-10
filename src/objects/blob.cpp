#include "blob.h"

#include "../hashing/sha256.h"

#include <utility>

Blob::Blob(std::string content)
    : content_(std::move(content))
{
}

const std::string& Blob::content() const
{
    return content_;
}

std::string Blob::serialized() const
{
    const std::string type = "blob";
    const auto size = content_.size();

    return type + " " +
           std::to_string(size) +
           '\0' +
           content_;
}

std::string Blob::id() const
{
    return sha256(serialized());
}