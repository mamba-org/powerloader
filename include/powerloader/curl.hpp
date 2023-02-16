#ifndef POWERLOADER_CURL_HPP
#define POWERLOADER_CURL_HPP

extern "C"
{
#include <curl/curl.h>
}

namespace powerloader
{
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
}

#endif
