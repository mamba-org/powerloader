#ifndef POWERLOADER_SRC_CURL_INTERNAL_HPP
#define POWERLOADER_SRC_CURL_INTERNAL_HPP

#include <powerloader/curl.hpp>

namespace powerloader::details
{
    // Scoped initialization and termination of CURL.
    // This should never have more than one instance live at any time.
    class CURLSetup final
    {
    public:
        explicit CURLSetup(const ssl_backend_t& ssl_backend)
        {
            auto res
                = curl_global_sslset((curl_sslbackend) ssl_backend, nullptr, nullptr);
            if (res == CURLSSLSET_UNKNOWN_BACKEND)
            {
                throw curl_error("unknown curl ssl backend");
            }
            else if (res == CURLSSLSET_NO_BACKENDS)
            {
                throw curl_error("no curl ssl backend available");
            }
            else if (res == CURLSSLSET_TOO_LATE)
            {
                throw curl_error("curl ssl backend set too late");
            }
            else if (res != CURLSSLSET_OK)
            {
                throw curl_error("failed to set curl ssl backend");
            }

            if (curl_global_init(CURL_GLOBAL_ALL) != 0)
                throw curl_error("failed to initialize curl");
        }

        ~CURLSetup()
        {
            curl_global_cleanup();
        }
    };
}
#endif