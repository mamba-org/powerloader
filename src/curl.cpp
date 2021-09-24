#include <cassert>
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

#include "curl.hpp"
#include "utils.hpp"
#include "context.hpp"

template <class T>
std::size_t
read_callback(char* ptr, std::size_t size, std::size_t nmemb, T* stream)
{
    // TODO stream error handling?!
    // copy as much data as possible into the 'ptr' buffer, but no more than
    // 'size' * 'nmemb' bytes!
    stream->read(ptr, size * nmemb);
    spdlog::info("Uploading {} bytes of data!", stream->gcount());
    return stream->gcount();
}

template std::size_t
read_callback<std::ifstream>(char* ptr, std::size_t size, std::size_t nmemb, std::ifstream* stream);
template std::size_t
read_callback<std::istringstream>(char* ptr,
                                  std::size_t size,
                                  std::size_t nmemb,
                                  std::istringstream* stream);

CURL*
get_handle()
{
    CURL* h;

    h = curl_easy_init();
    if (!h)
        return nullptr;

    if (curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1) != CURLE_OK)
        goto err;
    if (curl_easy_setopt(h, CURLOPT_MAXREDIRS, 6) != CURLE_OK)
        goto err;
    if (curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, LRO_CONNECTTIMEOUT_DEFAULT) != CURLE_OK)
        goto err;
    if (curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, LRO_LOWSPEEDTIME_DEFAULT) != CURLE_OK)
        goto err;
    if (curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, LRO_LOWSPEEDLIMIT_DEFAULT) != CURLE_OK)
        goto err;
    if (curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2) != CURLE_OK)
        goto err;
    if (curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1) != CURLE_OK)
        goto err;
    if (curl_easy_setopt(h, CURLOPT_SSL_VERIFYSTATUS, 0) != CURLE_OK)
        goto err;
    if (curl_easy_setopt(h, CURLOPT_FTP_USE_EPSV, LRO_FTPUSEEPSV_DEFAULT) != CURLE_OK)
        goto err;
    if (curl_easy_setopt(h, CURLOPT_FILETIME, 0) != CURLE_OK)
        goto err;

    if (Context::instance().verbosity > 0)
        if (curl_easy_setopt(h, CURLOPT_VERBOSE, 1) != CURLE_OK)
            goto err;

    return h;

err:
    curl_easy_cleanup(h);
    return nullptr;
}
