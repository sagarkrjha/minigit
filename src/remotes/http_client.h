#pragma once

#include <string>
#include <vector>

namespace minigit::remotes {

struct HttpResponse
{
    long status_code{0};
    std::string content_type;
    std::string body;
    std::string error;

    bool ok() const
    {
        return status_code >= 200 && status_code < 300 && error.empty();
    }
};

class HttpClient
{
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient &) = delete;
    HttpClient &operator=(const HttpClient &) = delete;

    HttpResponse get(
        const std::string &url,
        const std::vector<std::string> &headers = {}
    );

    HttpResponse post(
        const std::string &url,
        const std::string &body,
        const std::string &content_type,
        const std::vector<std::string> &headers = {}
    );

    void set_timeout_seconds(long timeout);

private:
    long timeout_seconds_{60};
};

} // namespace minigit::remotes
