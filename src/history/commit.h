#pragma once

#include <string>

// Create a commit from the current index.
// `message` is the commit message.
// `author` is the author string (e.g. "Name <email>").
void commit(const std::string& message, const std::string& author);
