#ifndef PW_CURL_HPP
#define PW_CURL_HPP

#include <filesystem>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/fmt/fmt.h>
#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include "utils.hpp"
#include "enums.hpp"

namespace powerloader
{
    class Context;

    extern "C"
    {
#include <curl/curl.h>
    }

    class curl_error : public std::runtime_error
    {
    public:
        curl_error(const std::string& what = "download error", bool serious = false);
        bool is_serious() const;

    private:
        bool m_serious;
    };

    struct Response
    {
        std::map<std::string, std::string> header;
        mutable std::stringstream content;

        curl_off_t avg_speed;
        curl_off_t downloaded_size;
        long http_status;
        std::string effective_url;

        bool ok() const;
        nlohmann::json json() const;
    };

    // TODO: rename this, try to not expose it
    CURL* get_handle(const Context& ctx);

    class CURLHandle
    {
    public:
        using end_callback_type = std::function<CbReturnCode(const Response&)>;
        explicit CURLHandle(const Context& ctx);
        CURLHandle(const Context& ctx, const std::string& url);
        ~CURLHandle();

        CURLHandle& url(const std::string& url);
        CURLHandle& accept_encoding();
        CURLHandle& user_agent(const std::string& user_agent);

        Response perform();
        void finalize_transfer();
        // TODO: should be private?
        void finalize_transfer(Response& response);

        template <class T>
        tl::expected<T, CURLcode> getinfo(CURLINFO option);

        // TODO: why do we need to expose these three methods
        CURL* handle();
        operator CURL*();
        CURL* ptr() const;

        CURLHandle& add_header(const std::string& header);
        CURLHandle& add_headers(const std::vector<std::string>& headers);
        CURLHandle& reset_headers();

        template <class T>
        CURLHandle& setopt(CURLoption opt, const T& val);

        void set_default_callbacks();
        CURLHandle& set_end_callback(end_callback_type func);

        CURLHandle& upload(std::ifstream& stream);
        CURLHandle& upload(std::istringstream& stream);

    private:
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
}

#endif
