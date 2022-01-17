#include <cassert>
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

#include "curl.hpp"
#include "utils.hpp"
#include "context.hpp"

namespace powerloader
{
    template <class T>
    std::size_t read_callback(char* ptr, std::size_t size, std::size_t nmemb, T* stream)
    {
        // copy as much data as possible into the 'ptr' buffer, but no more than
        // 'size' * 'nmemb' bytes!
        stream->read(ptr, size * nmemb);
        return stream->gcount();
    }

    template std::size_t read_callback<std::ifstream>(char* ptr,
                                                      std::size_t size,
                                                      std::size_t nmemb,
                                                      std::ifstream* stream);
    template std::size_t read_callback<std::istringstream>(char* ptr,
                                                           std::size_t size,
                                                           std::size_t nmemb,
                                                           std::istringstream* stream);

    CURL* get_handle()
    {
        CURL* h;

        auto& ctx = Context::instance();
        h = curl_easy_init();
        if (!h)
            return nullptr;

        if (curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1) != CURLE_OK)
            goto err;
        if (curl_easy_setopt(h, CURLOPT_MAXREDIRS, 6) != CURLE_OK)
            goto err;
        if (curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, ctx.connect_timeout) != CURLE_OK)
            goto err;
        if (curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, ctx.low_speed_time) != CURLE_OK)
            goto err;
        if (curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, ctx.low_speed_limit) != CURLE_OK)
            goto err;

        if (ctx.disable_ssl)
        {
            if (curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0) != CURLE_OK)
                goto err;
            if (curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0) != CURLE_OK)
                goto err;
        }
        else
        {
            if (curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2) != CURLE_OK)
                goto err;
            if (curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1) != CURLE_OK)
                goto err;

            // Windows SSL backend doesn't support this
            CURLcode verifystatus = curl_easy_setopt(h, CURLOPT_SSL_VERIFYSTATUS, 0);
            if (verifystatus != CURLE_OK && verifystatus != CURLE_NOT_BUILT_IN)
                goto err;
        }

        if (curl_easy_setopt(h, CURLOPT_FTP_USE_EPSV, (long) ctx.ftp_use_seepsv) != CURLE_OK)
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

}
