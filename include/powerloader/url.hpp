// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef POWERLOADER_URL_HPP
#define POWERLOADER_URL_HPP

extern "C"
{
#include <curl/urlapi.h>
}

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <powerloader/export.hpp>

namespace powerloader
{
    POWERLOADER_API bool has_scheme(const std::string& url);

    POWERLOADER_API bool compare_cleaned_url(const std::string& url1, const std::string& url2);

    POWERLOADER_API bool is_path(const std::string& input);
    POWERLOADER_API std::string path_to_url(const std::string& path);

    template <class S, class... Args>
    std::string join_url(const S& s, const Args&... args);

    POWERLOADER_API std::string unc_url(const std::string& url);
    POWERLOADER_API std::string encode_url(const std::string& url);
    POWERLOADER_API std::string decode_url(const std::string& url);
    // Only returns a cache name without extension
    POWERLOADER_API std::string cache_name_from_url(const std::string& url);

    class POWERLOADER_API URLHandler
    {
    public:
        URLHandler(const std::string& url = "");
        ~URLHandler();

        URLHandler(const URLHandler&);
        URLHandler& operator=(const URLHandler&);

        URLHandler(URLHandler&&);
        URLHandler& operator=(URLHandler&&);

        std::string url(bool strip_scheme = false) const;
        std::string url_without_path() const;

        std::string scheme() const;
        std::string host() const;
        std::string path() const;
        std::string port() const;

        std::string query() const;
        std::string fragment() const;
        std::string options() const;

        std::string auth() const;
        std::string user() const;
        std::string password() const;
        std::string zoneid() const;

        URLHandler& set_scheme(const std::string& scheme);
        URLHandler& set_host(const std::string& host);
        URLHandler& set_path(const std::string& path);
        URLHandler& set_port(const std::string& port);

        URLHandler& set_query(const std::string& query);
        URLHandler& set_fragment(const std::string& fragment);
        URLHandler& set_options(const std::string& options);

        URLHandler& set_user(const std::string& user);
        URLHandler& set_password(const std::string& password);
        URLHandler& set_zoneid(const std::string& zoneid);

    private:
        std::string get_part(CURLUPart part) const;
        void set_part(CURLUPart part, const std::string& s);

        std::string m_url;
        CURLU* m_handle;
        bool m_has_scheme;
    };

    namespace detail
    {
        inline std::string join_url_impl(std::string& s)
        {
            return s;
        }

        template <class S, class... Args>
        inline std::string join_url_impl(std::string& s1, const S& s2, const Args&... args)
        {
            if (!s2.empty())
            {
                s1 += '/' + s2;
            }
            return join_url_impl(s1, args...);
        }

        template <class... Args>
        inline std::string join_url_impl(std::string& s1, const char* s2, const Args&... args)
        {
            s1 += '/';
            s1 += s2;
            return join_url_impl(s1, args...);
        }
    }  // namespace detail

    template <class S, class... Args>
    inline std::string join_url(const S& s, const Args&... args)
    {
        std::string res = s;
        return detail::join_url_impl(res, args...);
    }
}  // namespace powerloader

#endif
