#include "http_client.h"

#include <curl/curl.h>

#include <cstdlib>
#include <iostream>

namespace minigit::remotes {

namespace {

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total_bytes = size * nmemb;
    auto *buffer = static_cast<std::string *>(userp);
    buffer->append(static_cast<const char *>(contents), total_bytes);
    return total_bytes;
}

bool should_skip_ssl_verify()
{
    const char *val1 = std::getenv("GIT_SSL_NO_VERIFY");
    if (val1 && (std::string(val1) == "1" || std::string(val1) == "true" || std::string(val1) == "TRUE"))
        return true;
    const char *val2 = std::getenv("MINIGIT_SSL_NO_VERIFY");
    if (val2 && (std::string(val2) == "1" || std::string(val2) == "true" || std::string(val2) == "TRUE"))
        return true;
    return false;
}

struct CurlGlobalInitializer
{
    CurlGlobalInitializer()
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    ~CurlGlobalInitializer()
    {
        curl_global_cleanup();
    }
};

void ensure_curl_initialized()
{
    static CurlGlobalInitializer init;
}

struct SlistGuard
{
    struct curl_slist *list{nullptr};
    ~SlistGuard()
    {
        if (list)
            curl_slist_free_all(list);
    }
};

struct CurlGuard
{
    CURL *curl{nullptr};
    ~CurlGuard()
    {
        if (curl)
            curl_easy_cleanup(curl);
    }
};

} // namespace

HttpClient::HttpClient()
{
    ensure_curl_initialized();
}

HttpClient::~HttpClient() = default;

void HttpClient::set_timeout_seconds(long timeout)
{
    timeout_seconds_ = timeout;
}

HttpResponse HttpClient::get(
    const std::string &url,
    const std::vector<std::string> &headers)
{
    HttpResponse response;
    CurlGuard curl_guard{curl_easy_init()};
    CURL *curl = curl_guard.curl;
    if (!curl)
    {
        response.error = "Failed to initialize CURL easy handle";
        return response;
    }

    SlistGuard chunk_guard;
    for (const auto &h : headers)
    {
        chunk_guard.list = curl_slist_append(chunk_guard.list, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "minigit/1.8.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    if (chunk_guard.list)
    {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk_guard.list);
    }

    if (should_skip_ssl_verify())
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        response.error = curl_easy_strerror(res);
    }
    else
    {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
        char *ct = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
        if (ct)
            response.content_type = ct;
    }

    return response;
}

HttpResponse HttpClient::post(
    const std::string &url,
    const std::string &body,
    const std::string &content_type,
    const std::vector<std::string> &headers)
{
    HttpResponse response;
    CurlGuard curl_guard{curl_easy_init()};
    CURL *curl = curl_guard.curl;
    if (!curl)
    {
        response.error = "Failed to initialize CURL easy handle";
        return response;
    }

    SlistGuard chunk_guard;
    if (!content_type.empty())
    {
        std::string ct_hdr = "Content-Type: " + content_type;
        chunk_guard.list = curl_slist_append(chunk_guard.list, ct_hdr.c_str());
    }
    for (const auto &h : headers)
    {
        chunk_guard.list = curl_slist_append(chunk_guard.list, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "minigit/1.8.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    if (chunk_guard.list)
    {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk_guard.list);
    }

    if (should_skip_ssl_verify())
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        response.error = curl_easy_strerror(res);
    }
    else
    {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
        char *ct = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
        if (ct)
            response.content_type = ct;
    }

    return response;
}

} // namespace minigit::remotes
