#include <cassert>
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

#include "curl.hpp"
#include "utils.hpp"
#include "context.hpp"

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

    /************
     * Response *
     ************/

    bool Response::ok() const
    {
        return http_status / 100 == 2;
    }

    nlohmann::json Response::json() const
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
            throw;
        }
    }

    /**************
     * CURLHandle*
     **************/

    CURL* get_handle()
    {
        // TODO: get rid of goto
        auto& ctx = Context::instance();
        CURL* h = curl_easy_init();
        if (!h)
            return nullptr;

        if (curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1) != CURLE_OK)
            goto err;
        if (curl_easy_setopt(h, CURLOPT_MAXREDIRS, 6) != CURLE_OK)
            goto err;
        if (curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, ctx.connect_timeout) != CURLE_OK)
            goto err;
        if (curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, ctx.low_speed_time) != CURLE_OK)
            goto err;
        if (curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, ctx.low_speed_limit) != CURLE_OK)
            goto err;

        if (ctx.disable_ssl)
        {
            if (curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0) != CURLE_OK)
                goto err;
            if (curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0) != CURLE_OK)
                goto err;
        }
        else
        {
            if (curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2) != CURLE_OK)
                goto err;
            if (curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1) != CURLE_OK)
                goto err;

            // Windows SSL backend doesn't support this
            CURLcode verifystatus = curl_easy_setopt(h, CURLOPT_SSL_VERIFYSTATUS, 0);
            if (verifystatus != CURLE_OK && verifystatus != CURLE_NOT_BUILT_IN)
                goto err;
        }

        if (curl_easy_setopt(h, CURLOPT_FTP_USE_EPSV, (long) ctx.ftp_use_seepsv) != CURLE_OK)
            goto err;

        if (curl_easy_setopt(h, CURLOPT_FILETIME, (long) ctx.preserve_filetime) != CURLE_OK)
            goto err;

        if (Context::instance().verbosity > 0)
            if (curl_easy_setopt(h, CURLOPT_VERBOSE, 1) != CURLE_OK)
                goto err;

        return h;

    err:
        curl_easy_cleanup(h);
        return nullptr;
    }

    CURLHandle::CURLHandle()
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

    CURLHandle::CURLHandle(const std::string& url)
        : CURLHandle()
    {
        this->url(url);
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

    CURLHandle& CURLHandle::url(const std::string& url)
    {
        setopt(CURLOPT_URL, url.c_str());
        return *this;
    }

    CURLHandle& CURLHandle::accept_encoding()
    {
        setopt(CURLOPT_ACCEPT_ENCODING, nullptr);
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
            throw curl_error(
                fmt::format("{} [{}]", curl_easy_strerror(curl_result), errorbuffer));
        }
        finalize_transfer(*response);
        return std::move(*response.release());
    }

    void CURLHandle::finalize_transfer()
    {
        if (response)
            finalize_transfer(*response);
    }

    void CURLHandle::finalize_transfer(Response& response)
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

        if (!response.ok())
        {
            spdlog::error("Received {}: {}", response.http_status, response.content.str());
        }
        if (end_callback)
        {
            end_callback(response);
        }
    }

    template <class T>
    cpp::result<T, CURLcode> CURLHandle::getinfo(CURLINFO option)
    {
        T val;
        CURLcode result = curl_easy_getinfo(m_handle, option, &val);
        if (result != CURLE_OK)
            return cpp::fail(result);
        return val;
    }

    template cpp::result<long, CURLcode> CURLHandle::getinfo(CURLINFO option);
    template cpp::result<char*, CURLcode> CURLHandle::getinfo(CURLINFO option);
    template cpp::result<double, CURLcode> CURLHandle::getinfo(CURLINFO option);
    template cpp::result<curl_slist*, CURLcode> CURLHandle::getinfo(CURLINFO option);

    CURL* CURLHandle::handle()
    {
        if (p_headers)
            setopt(CURLOPT_HTTPHEADER, p_headers);
        return m_handle;
    }

    CURLHandle::operator CURL*()
    {
        return handle();
    }

    CURL* CURLHandle::ptr() const
    {
        return m_handle;
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
        std::size_t ostream_callback(char* buffer,
                                     std::size_t size,
                                     std::size_t nitems,
                                     T* stream)
        {
            stream->write(buffer, size * nitems);
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
        setopt(CURLOPT_HEADERDATA, &response->header);

        setopt(CURLOPT_WRITEFUNCTION, ostream_callback<std::stringstream>);
        setopt(CURLOPT_WRITEDATA, &response->content);
    }

    CURLHandle& CURLHandle::set_end_callback(const end_callback_type& func)
    {
        end_callback = func;
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

}
