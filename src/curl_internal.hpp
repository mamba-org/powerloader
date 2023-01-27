#ifndef POWERLOADER_SRC_CURL_INTERNAL_HPP
#define POWERLOADER_SRC_CURL_INTERNAL_HPP

#include <optional>

#include <powerloader/curl.hpp>

namespace powerloader::details
{
    // Scoped initialization and termination of CURL.
    // This should never have more than one instance live at any time,
    // this object's constructor will throw an `std::runtime_error` if it's the case.
    class CURLSetup final
    {
    public:
        explicit CURLSetup(const ssl_backend_t& ssl_backend);
        ~CURLSetup();

        CURLSetup(CURLSetup&&) = delete;
        CURLSetup& operator=(CURLSetup&&) = delete;

        CURLSetup(const CURLSetup&) = delete;
        CURLSetup& operator=(const CURLSetup&) = delete;
    };
}
#endif
