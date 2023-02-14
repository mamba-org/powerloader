#include <cassert>
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

#include <powerloader/curl.hpp>
#include <powerloader/utils.hpp>
#include <powerloader/context.hpp>
#include <powerloader/url.hpp>

#include "curl_internal.hpp"

namespace powerloader
{
    /**************
     * curl_error *
     **************/

    curl_error::curl_error(const std::string& what, bool serious)
        : std::runtime_error(what)
        , m_serious(serious)
    {
    }

    bool curl_error::is_serious() const
    {
        return m_serious;
    }


    /**************
     * CURLHandle*
     **************/

    CURLHandle::CURLHandle(const Context& ctx)
        : m_handle(curl_easy_init())
    {
        if (m_handle == nullptr)
        {
            throw curl_error("Could not initialize CURL handle");
        }

        init_handle(ctx);
        // Set error buffer
        errorbuffer[0] = '\0';
        setopt(CURLOPT_ERRORBUFFER, errorbuffer);
    }

    void CURLHandle::init_handle(const Context& ctx)
    {
        setopt(CURLOPT_FOLLOWLOCATION, 1);
        setopt(CURLOPT_NETRC, CURL_NETRC_OPTIONAL);
        setopt(CURLOPT_MAXREDIRS, 6);
        setopt(CURLOPT_CONNECTTIMEOUT, ctx.connect_timeout);
        setopt(CURLOPT_LOW_SPEED_TIME, ctx.low_speed_time);
        setopt(CURLOPT_LOW_SPEED_LIMIT, ctx.low_speed_limit);
        setopt(CURLOPT_BUFFERSIZE, ctx.transfer_buffersize);

        if (ctx.disable_ssl)
        {
            spdlog::warn("SSL verification is disabled");
            setopt(CURLOPT_SSL_VERIFYHOST, 0);
            setopt(CURLOPT_SSL_VERIFYPEER, 0);

            // also disable proxy SSL verification
            setopt(CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
            setopt(CURLOPT_PROXY_SSL_VERIFYHOST, 0L);
        }
        else
        {
            spdlog::warn("SSL verification is ENABLED");

            setopt(CURLOPT_SSL_VERIFYHOST, 2);
            setopt(CURLOPT_SSL_VERIFYPEER, 1);

            // Windows SSL backend doesn't support this
            CURLcode verifystatus = curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYSTATUS, 0);
            if (verifystatus != CURLE_OK && verifystatus != CURLE_NOT_BUILT_IN)
                throw curl_error("Could not initialize CURL handle");

            if (!ctx.ssl_ca_info.empty())
            {
                setopt(CURLOPT_CAINFO, ctx.ssl_ca_info.c_str());
            }

            if (ctx.ssl_no_revoke)
            {
                setopt(CURLOPT_SSL_OPTIONS, ctx.ssl_no_revoke);
            }
        }

        setopt(CURLOPT_FTP_USE_EPSV, (long) ctx.ftp_use_seepsv);
        setopt(CURLOPT_FILETIME, (long) ctx.preserve_filetime);

        if (ctx.verbosity > 0)
            setopt(CURLOPT_VERBOSE, (long) 1L);
    }

    CURLHandle::CURLHandle(const Context& ctx, const std::string& url)
        : CURLHandle(ctx)
    {
        this->url(url, ctx.proxy_map);
    }

    CURLHandle::~CURLHandle()
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

    CURLHandle::CURLHandle(CURLHandle&& rhs)
        : m_handle(std::move(rhs.m_handle))
        , p_headers(std::move(rhs.p_headers))
        , response(std::move(rhs.response))
        , end_callback(std::move(rhs.end_callback))
    {
        std::copy(&rhs.errorbuffer[0], &rhs.errorbuffer[CURL_ERROR_SIZE], &errorbuffer[0]);
    }

    CURLHandle& CURLHandle::operator=(CURLHandle&& rhs)
    {
        using std::swap;
        swap(m_handle, rhs.m_handle);
        swap(p_headers, rhs.p_headers);
        swap(errorbuffer, rhs.errorbuffer);
        swap(response, rhs.response);
        swap(end_callback, rhs.end_callback);
        return *this;
    }

    CURLHandle& CURLHandle::url(const std::string& url, const proxy_map_type& proxies)
    {
        setopt(CURLOPT_URL, url.c_str());
        const auto match = proxy_match(proxies, url);
        if (match)
        {
            setopt(CURLOPT_PROXY, match.value().c_str());
        }
        else
        {
            setopt(CURLOPT_PROXY, nullptr);
        }
        return *this;
    }

    CURLHandle& CURLHandle::accept_encoding()
    {
        setopt(CURLOPT_ACCEPT_ENCODING, "");
        return *this;
    }

    CURLHandle& CURLHandle::user_agent(const std::string& user_agent)
    {
        add_header(fmt::format("User-Agent: {} {}", user_agent, curl_version()));
        return *this;
    }

    Response CURLHandle::perform()
    {
        set_default_callbacks();
        CURLcode curl_result = curl_easy_perform(handle());
        if (curl_result != CURLE_OK)
        {
            throw curl_error(fmt::format("{} [{}]", curl_easy_strerror(curl_result), errorbuffer));
        }
        finalize_transfer(*response);
        return std::move(*response.release());
    }

    void CURLHandle::finalize_transfer()
    {
        if (response)
            finalize_transfer(*response);
    }

    void CURLHandle::finalize_transfer(Response& lresponse)
    {
        lresponse.fill_values(*this);
        if (!lresponse.ok())
        {
            spdlog::error("Received {}: {}", lresponse.http_status, lresponse.content.value());
        }
        if (end_callback)
        {
            end_callback(lresponse);
        }
    }

    template <class T>
    tl::expected<T, CURLcode> CURLHandle::getinfo(CURLINFO option)
    {
        T val;
        CURLcode result = curl_easy_getinfo(m_handle, option, &val);
        if (result != CURLE_OK)
            return tl::unexpected(result);
        return val;
    }

    template tl::expected<long, CURLcode> CURLHandle::getinfo(CURLINFO option);
    template tl::expected<char*, CURLcode> CURLHandle::getinfo(CURLINFO option);
    template tl::expected<double, CURLcode> CURLHandle::getinfo(CURLINFO option);
    template tl::expected<curl_slist*, CURLcode> CURLHandle::getinfo(CURLINFO option);
    template tl::expected<long long, CURLcode> CURLHandle::getinfo(CURLINFO option);

    template <>
    tl::expected<std::string, CURLcode> CURLHandle::getinfo(CURLINFO option)
    {
        auto res = getinfo<char*>(option);
        if (res)
            return std::string(res.value());
        else
            return tl::unexpected(res.error());
    }

    CURL* CURLHandle::handle()
    {
        if (p_headers)
            setopt(CURLOPT_HTTPHEADER, p_headers);
        return m_handle;
    }

    bool CURLHandle::handle_exists()
    {
        return (handle() != nullptr);
    }

    CURLHandle& CURLHandle::add_header(const std::string& header)
    {
        p_headers = curl_slist_append(p_headers, header.c_str());
        if (!p_headers)
        {
            throw std::bad_alloc();
        }
        return *this;
    }

    CURLHandle& CURLHandle::add_headers(const std::vector<std::string>& headers)
    {
        for (auto& h : headers)
        {
            add_header(h);
        }
        return *this;
    }

    CURLHandle& CURLHandle::reset_headers()
    {
        curl_slist_free_all(p_headers);
        p_headers = nullptr;
        return *this;
    }

    namespace
    {
        template <class T>
        std::size_t ostream_callback(char* buffer, std::size_t size, std::size_t nitems, T* stream)
        {
            stream->write(buffer, size * nitems);
            return size * nitems;
        }

        template <class T>
        std::size_t string_callback(char* buffer, std::size_t size, std::size_t nitems, T* string)
        {
            string->append(buffer, size * nitems);
            return size * nitems;
        }

        template <class T>
        std::size_t header_map_callback(char* buffer,
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
    }

    void CURLHandle::set_default_callbacks()
    {
        response.reset(new Response);
        // check if there is something set already for these values
        setopt(CURLOPT_HEADERFUNCTION, header_map_callback<std::map<std::string, std::string>>);
        setopt(CURLOPT_HEADERDATA, &response->headers);

        setopt(CURLOPT_WRITEFUNCTION, string_callback<std::string>);
        response->content = std::string();
        setopt(CURLOPT_WRITEDATA, &response->content.value());
    }

    CURLHandle& CURLHandle::set_end_callback(end_callback_type func)
    {
        end_callback = std::move(func);
        return *this;
    }

    namespace
    {
        template <class T>
        std::size_t read_callback(char* ptr, std::size_t size, std::size_t nmemb, T* stream)
        {
            // copy as much data as possible into the 'ptr' buffer, but no more than
            // 'size' * 'nmemb' bytes!
            stream->read(ptr, size * nmemb);
            return stream->gcount();
        }

        template <class S>
        CURLHandle& upload_impl(CURLHandle& curl, S& stream)
        {
            stream.seekg(0, stream.end);
            curl_off_t fsize = stream.tellg();
            stream.seekg(0, stream.beg);

            if (fsize != -1)
            {
                curl.setopt(CURLOPT_INFILESIZE_LARGE, fsize);
            }

            curl.setopt(CURLOPT_UPLOAD, 1L);
            curl.setopt(CURLOPT_READFUNCTION, read_callback<S>);
            curl.setopt(CURLOPT_READDATA, &stream);
            return curl;
        }
    }


    CURLHandle& CURLHandle::upload(std::ifstream& stream)
    {
        return upload_impl(*this, stream);
    }

    CURLHandle& CURLHandle::upload(std::istringstream& stream)
    {
        return upload_impl(*this, stream);
    }

    /************
     * Response *
     ************/

    bool Response::ok() const
    {
        return http_status / 100 == 2;
    }

    tl::expected<std::string, std::out_of_range> Response::get_header(
        const std::string& header) const
    {
        if (headers.find(header) != headers.end())
            return headers.at(header);
        else
            return tl::unexpected(
                std::out_of_range(std::string("Could not find header ") + header));
    }

    nlohmann::json Response::json() const
    {
        try
        {
            return nlohmann::json::parse(content.value());
        }
        catch (const nlohmann::detail::parse_error& e)
        {
            spdlog::error("Could not parse JSON\n{}", content.value());
            spdlog::error("Error message: {}", e.what());
            throw;
        }
    }

    void Response::fill_values(CURLHandle& handle)
    {
        average_speed = CURLInterface::get_info_wrapped<decltype(average_speed)>(
                            handle, CURLINFO_SPEED_DOWNLOAD_T)
                            .value_or(0);
        http_status
            = CURLInterface::get_info_wrapped<decltype(http_status)>(handle, CURLINFO_RESPONSE_CODE)
                  .value();
        effective_url = CURLInterface::get_info_wrapped<decltype(effective_url)>(
                            handle, CURLINFO_EFFECTIVE_URL)
                            .value();
        downloaded_size = CURLInterface::get_info_wrapped<decltype(downloaded_size)>(
                              handle, CURLINFO_SIZE_DOWNLOAD_T)
                              .value();
    }

    std::optional<std::string> proxy_match(const proxy_map_type& proxies, const std::string& url)
    {
        // This is a reimplementation of requests.utils.select_proxy()
        // of the python requests library used by conda
        if (proxies.empty())
        {
            return std::nullopt;
        }

        auto handler = URLHandler(url);
        auto scheme = handler.scheme();
        auto host = handler.host();
        std::vector<std::string> options;

        if (host.empty())
        {
            options = {
                scheme,
                "all",
            };
        }
        else
        {
            options = { scheme + "://" + host, scheme, "all://" + host, "all" };
        }

        for (auto& option : options)
        {
            auto proxy = proxies.find(option);
            if (proxy != proxies.end())
            {
                return proxy->second;
            }
        }

        return std::nullopt;
    }

    namespace details
    {
        static std::atomic<bool> is_curl_setup_alive{ false };

        CURLSetup::CURLSetup(const ssl_backend_t& ssl_backend)
        {
            {
                bool expected = false;
                if (!is_curl_setup_alive.compare_exchange_strong(expected, true))
                    throw std::runtime_error(
                        "powerloader::CURLSetup created more than once - instance must be unique");
            }

            const auto res = curl_global_sslset((curl_sslbackend) ssl_backend, nullptr, nullptr);
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

        CURLSetup::~CURLSetup()
        {
            curl_global_cleanup();
            is_curl_setup_alive = false;
        }
    }

    CURLMcode CURLInterface::multi_add_handle(CURLM* multi_handle, CURLHandle& h)
    {
        return curl_multi_add_handle(multi_handle, h.handle());
    }

    void CURLInterface::multi_remove_handle(CURLM* multihandle, CURLHandle& h)
    {
        curl_multi_remove_handle(multihandle, h.handle());
    }

    bool CURLInterface::handle_is_equal(CURLHandle* h, CURLMsg* msg)
    {
        return (h->handle() == msg->easy_handle);
    }

    template <class T>
    tl::expected<T, CURLcode> CURLInterface::get_info_wrapped(CURLHandle& h, CURLINFO option)
    {
        return h.getinfo<T>(option);
    }
}
