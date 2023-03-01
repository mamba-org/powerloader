#ifndef POWERLOADER_SRC_CURL_INTERNAL_HPP
#define POWERLOADER_SRC_CURL_INTERNAL_HPP

#include <sstream>
#include <string>
#include <vector>

#include <fmt/core.h>

#include <powerloader/export.hpp>
#include <powerloader/utils.hpp>
#include <powerloader/enums.hpp>
#include <powerloader/curl.hpp>

namespace powerloader
{
    class Context;
    using proxy_map_type = std::map<std::string, std::string>;

    // If needed, this can be moved to curl.hpp
    class POWERLOADER_API curl_error : public std::runtime_error
    {
    public:
        curl_error(const std::string& what = "download error", bool serious = false);
        bool is_serious() const;

    private:
        bool m_serious;
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

        template <class T>
        tl::expected<T, CURLcode> getinfo(CURLINFO option);

        // This is made public because it is used internally in quite some files
        CURL* handle();

        CURLHandle& add_header(const std::string& header);
        CURLHandle& add_headers(const std::vector<std::string>& headers);
        CURLHandle& reset_headers();

        template <class T>
        CURLHandle& setopt(CURLoption opt, const T& val);

        void set_default_callbacks();
        CURLHandle& set_end_callback(end_callback_type func);

        CURLHandle& upload(std::ifstream& stream);
        CURLHandle& upload(std::istringstream& stream);

        CURLHandle(CURLHandle&& rhs);
        CURLHandle& operator=(CURLHandle&& rhs);

    private:
        void init_handle(const Context& ctx);
        void finalize_transfer(Response& response);

        CURL* m_handle;
        curl_slist* p_headers = nullptr;
        char errorbuffer[CURL_ERROR_SIZE];

        std::unique_ptr<Response> response;
        end_callback_type end_callback;
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
