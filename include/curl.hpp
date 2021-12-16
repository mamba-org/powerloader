#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/fmt/fmt.h>
#include <nlohmann/json.hpp>

#include "utils.hpp"

namespace powerloader
{
    class curl_error : public std::runtime_error
    {
    public:
        curl_error(const std::string& what = "download error", bool serious = false)
            : std::runtime_error(what)
            , serious(serious)
        {
        }
        bool serious;
    };

    namespace fs = std::filesystem;

    extern "C"
    {
#include <curl/curl.h>
    }

#define LRO_CONNECTTIMEOUT_DEFAULT 30L
#define LRO_LOWSPEEDTIME_DEFAULT 30L
#define LRO_LOWSPEEDLIMIT_DEFAULT 1000L
#define LRO_FTPUSEEPSV_DEFAULT 1L

    template <class T>
    std::size_t read_callback(char* ptr, std::size_t size, std::size_t nmemb, T* stream);

    CURL* get_handle();

    struct Response
    {
        int status_code;
        std::map<std::string, std::string> header;
        mutable std::stringstream content;

        curl_off_t avg_speed, downloaded_size;
        long http_status;
        std::string effective_url;

        inline nlohmann::json json() const
        {
            try
            {
                nlohmann::json j;
                content >> j;
                return j;
            }
            catch (const nlohmann::detail::parse_error& e)
            {
                spdlog::error("Could not parse JSON\n{}", content.str());
                spdlog::error("Error message: {}", e.what());
                exit(1);
            }
        }
    };

    template <class T>
    static std::size_t ostream_callback(char* buffer,
                                        std::size_t size,
                                        std::size_t nitems,
                                        T* stream)
    {
        stream->write(buffer, size * nitems);
        return size * nitems;
    }

    template <class T>
    static std::size_t header_map_callback(char* buffer,
                                           std::size_t size,
                                           std::size_t nitems,
                                           T* header_map)
    {
        auto kv = parse_header(std::string_view(buffer, size * nitems));
        if (!kv.first.empty())
        {
            (*header_map)[kv.first] = kv.second;
        }
        return size * nitems;
    }

    class CURLHandle
    {
    public:
        CURLHandle()
            : m_handle(get_handle())
        {
            if (m_handle == nullptr)
            {
                throw curl_error("Could not initialize handle");
            }
            // Set error buffer
            errorbuffer[0] = '\0';
            setopt(CURLOPT_ERRORBUFFER, errorbuffer);
        }

        CURLHandle(const std::string& url)
            : CURLHandle()
        {
            this->url(url);
        }

        CURLHandle& url(const std::string& url)
        {
            setopt(CURLOPT_URL, url.c_str());
            return *this;
        }

        CURLHandle& accept_encoding()
        {
            setopt(CURLOPT_ACCEPT_ENCODING, nullptr);
            return *this;
        }

        CURLHandle& user_agent(const std::string& user_agent)
        {
            add_header(fmt::format("User-Agent: {} {}", user_agent, curl_version()));
            return *this;
        }

        // CURLHandle& exists_only(bool use_get = false)
        // {
        //     setopt(CURLOPT_FAILONERROR, 1L);
        //     if (use_get)
        //         setopt(CURLOPT_NOBODY, 0L);
        //     else
        //         setopt(CURLOPT_NOBODY, 1L);

        //     return *this;
        // }

        inline Response perform()
        {
            set_default_callbacks();
            // TODO error handling
            int curl_result = curl_easy_perform(handle());
            finalize_transfer(*response);
            return std::move(*response.release());
        }

        inline CURL* handle()
        {
            if (p_headers)
                setopt(CURLOPT_HTTPHEADER, p_headers);
            return m_handle;
        }

        inline void finalize_transfer()
        {
            if (response)
                finalize_transfer(*response);
        }

        template <class T>
        inline void finalize_transfer(T& response)
        {
            auto cres = curl_easy_getinfo(m_handle, CURLINFO_SPEED_DOWNLOAD_T, &response.avg_speed);
            if (cres != CURLE_OK)
            {
                response.avg_speed = 0;
            }

            char* tmp_effective_url;
            // TODO error handling?!
            curl_easy_getinfo(m_handle, CURLINFO_RESPONSE_CODE, &response.http_status);
            curl_easy_getinfo(m_handle, CURLINFO_EFFECTIVE_URL, &tmp_effective_url);
            curl_easy_getinfo(m_handle, CURLINFO_SIZE_DOWNLOAD_T, &response.downloaded_size);

            response.effective_url = tmp_effective_url;

            if (end_callback)
            {
                end_callback(response);
            }
        }

        inline operator CURL*()
        {
            return handle();
        }

        inline CURLHandle& add_header(const std::string& header)
        {
            p_headers = curl_slist_append(p_headers, header.c_str());
            if (!p_headers)
            {
                throw std::bad_alloc();
            }
            return *this;
        }

        inline CURLHandle& add_headers(const std::vector<std::string>& headers)
        {
            for (auto& h : headers)
            {
                add_header(h);
            }
            return *this;
        }

        inline CURLHandle& reset_headers()
        {
            curl_slist_free_all(p_headers);
            p_headers = nullptr;
            return *this;
        }

        template <class T>
        inline CURLHandle& setopt(CURLoption opt, const T& val)
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

        ~CURLHandle()
        {
            if (m_handle)
            {
                curl_easy_cleanup(m_handle);
            }
            if (p_headers)
            {
                curl_slist_free_all(p_headers);
            }
        }

        inline CURL* ptr() const
        {
            return m_handle;
        }

        inline void set_default_callbacks()
        {
            response.reset(new Response);
            // check if there is something set already for these values
            setopt(CURLOPT_HEADERFUNCTION, header_map_callback<std::map<std::string, std::string>>);
            setopt(CURLOPT_HEADERDATA, &response->header);

            setopt(CURLOPT_WRITEFUNCTION, ostream_callback<std::stringstream>);
            setopt(CURLOPT_WRITEDATA, &response->content);
        }

        template <class S>
        inline CURLHandle& upload(S& stream)
        {
            stream.seekg(0, stream.end);
            curl_off_t fsize = stream.tellg();
            stream.seekg(0, stream.beg);

            if (fsize != -1)
            {
                setopt(CURLOPT_INFILESIZE_LARGE, fsize);
            }

            setopt(CURLOPT_UPLOAD, 1L);
            setopt(CURLOPT_READFUNCTION, read_callback<S>);
            setopt(CURLOPT_READDATA, &stream);
            return *this;
        }

        inline CURLHandle& set_end_callback(const std::function<int(const Response&)>& func)
        {
            end_callback = func;
            return *this;
        }

        CURL* m_handle;
        curl_slist* p_headers = nullptr;
        char errorbuffer[CURL_ERROR_SIZE];

        std::unique_ptr<Response> response;
        std::function<int(const Response&)> end_callback;
    };

}
