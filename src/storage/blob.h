#pragma once

#include <string>

class Blob
{
private:
    /* data */
    std::string content_;
public:
    explicit Blob(std::string content);
    const std::string & content() const;
    std::string serialized() const;
    std::string id() const;
};