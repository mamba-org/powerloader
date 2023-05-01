#ifndef POWERLOADER_CURL_HPP
#define POWERLOADER_CURL_HPP

#include <filesystem>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <optional>

#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

extern "C"
{
#include <curl/curl.h>
}

#include <powerloader/export.hpp>
#include <powerloader/utils.hpp>
#include <powerloader/enums.hpp>

namespace powerloader
{
    class Context;
    class CURLHandle;
    using proxy_map_type = std::map<std::string, std::string>;

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

    class POWERLOADER_API curl_error : public std::runtime_error
    {
    public:
        curl_error(const std::string& what = "download error", bool serious = false);
        bool is_serious() const;

    private:
        bool m_serious;
    };

    struct POWERLOADER_API Response
    {
        std::map<std::string, std::string> headers;

        curl_off_t average_speed = -1;
        curl_off_t downloaded_size = -1;
        long http_status = 0;
        std::string effective_url;

        bool ok() const;

        tl::expected<std::string, std::out_of_range> get_header(const std::string& header) const;

        void fill_values(CURLHandle& handle);

        // These are only working _if_ you are filling the content (e.g. by using the default
        // `h.perform() method)
        std::optional<std::string> content;
        nlohmann::json json() const;
    };

    class POWERLOADER_API CURLHandle
    {
    public:
        using end_callback_type = std::function<CbReturnCode(const Response&)>;
        explicit CURLHandle(const Context& ctx);
        CURLHandle(const Context& ctx, const std::string& url);
        ~CURLHandle();

        CURLHandle& url(const std::string& url, const proxy_map_type& proxies);
        CURLHandle& accept_encoding();
        CURLHandle& user_agent(const std::string& user_agent);

        Response perform();
        void finalize_transfer();

        bool handle_exists();

        CURLHandle& add_header(const std::string& header);
        CURLHandle& add_headers(const std::vector<std::string>& headers);
        CURLHandle& reset_headers();

        void set_default_callbacks();
        CURLHandle& set_end_callback(end_callback_type func);

        CURLHandle& upload(std::ifstream& stream);
        CURLHandle& upload(std::istringstream& stream);

        CURLHandle(CURLHandle&& rhs);
        CURLHandle& operator=(CURLHandle&& rhs);

    private:
        void init_handle(const Context& ctx);
        void finalize_transfer(Response& response);

        CURL* handle();

        template <class T>
        tl::expected<T, CURLcode> getinfo(CURLINFO option);

        template <class T>
        CURLHandle& setopt(CURLoption opt, const T& val);

        CURL* m_handle;
        curl_slist* p_headers = nullptr;
        char errorbuffer[CURL_ERROR_SIZE];

        std::unique_ptr<Response> response;
        end_callback_type end_callback;

        friend class CURLInterface;

        friend void add_multipart_upload(CURLHandle& target,
                                         const std::vector<std::string>& files,
                                         const std::map<std::string, std::string>& extra_fields);
    };

    // TODO: restrict the possible implementations in the cpp file
    template <class T>
    CURLHandle& CURLHandle::setopt(CURLoption opt, const T& val)
    {
        CURLcode ok;
        if constexpr (std::is_same<T, std::string>())
        {
            ok = curl_easy_setopt(m_handle, opt, val.c_str());
        }
        else if constexpr (std::is_same<T, bool>())
        {
            ok = curl_easy_setopt(m_handle, opt, val ? 1L : 0L);
        }
        else
        {
            ok = curl_easy_setopt(m_handle, opt, val);
        }
        if (ok != CURLE_OK)
        {
            throw curl_error(
                fmt::format("curl: curl_easy_setopt failed {}", curl_easy_strerror(ok)));
        }
        return *this;
    }

    std::optional<std::string> proxy_match(const proxy_map_type& ctx, const std::string& url);
}

#endif
