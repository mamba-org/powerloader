#ifndef POWERLOADER_CURL_HPP
#define POWERLOADER_CURL_HPP

#include <map>
#include <optional>

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

extern "C"
{
#include <curl/curl.h>
}

namespace powerloader
{
    class CURLHandle;

    enum class ssl_backend_t
    {
        none = CURLSSLBACKEND_NONE,
        openssl = CURLSSLBACKEND_OPENSSL,
        gnutls = CURLSSLBACKEND_GNUTLS,
        nss = CURLSSLBACKEND_NSS,
        gskit = CURLSSLBACKEND_GSKIT,
        // polarssl = CURLSSLBACKEND_POLARSSL /* deprecated by curl */,
        wolfssl = CURLSSLBACKEND_WOLFSSL,
        schannel = CURLSSLBACKEND_SCHANNEL,
        securetransport = CURLSSLBACKEND_SECURETRANSPORT,
        // axtls = CURLSSLBACKEND_AXTLS, /* deprecated by curl */
        mbedtls = CURLSSLBACKEND_MBEDTLS,
        // mesalink = CURLSSLBACKEND_MESALINK, /* deprecated by curl */
        bearssl = CURLSSLBACKEND_BEARSSL,
        rustls = CURLSSLBACKEND_RUSTLS,
    };

    // This is supposed to be used outside powerloader
    struct POWERLOADER_API Response
    {
        std::map<std::string, std::string> headers;

        long http_status = 0;
        std::string effective_url;

        bool ok() const;

        tl::expected<std::string, std::out_of_range> get_header(const std::string& header) const;

        void fill_values(CURLHandle& handle);

        // These are only working _if_ you are filling the content (e.g. by using the default
        // `h.perform() method)
        std::optional<std::string> content;
        nlohmann::json json() const;

    private:
        curl_off_t average_speed = -1;
        curl_off_t downloaded_size = -1;
    };

}

#endif
