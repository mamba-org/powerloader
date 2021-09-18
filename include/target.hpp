#pragma once

#include <filesystem>
#include <set>
#include <fstream>
#include <fmt/core.h>

namespace fs = std::filesystem;

#include "curl.hpp"
#include "utils.hpp"
#include "enums.hpp"
#include "mirror.hpp"
#include "download_target.hpp"

class Target
{
public:
    /** Header callback for CURL handles.
     * It parses HTTP and FTP headers and try to find length of the content
     * (file size of the target). If the size is different then the expected
     * size, then the transfer is interrupted.
     * This callback is used only if the expected size is specified.
     */
    static std::size_t header_callback(char *buffer, std::size_t size, std::size_t nitems, Target *self);
    static std::size_t write_callback(char *buffer, std::size_t size, std::size_t nitems, Target *self);

    inline Target(DownloadTarget *dl_target)
        : state(DownloadState::WAITING), target(dl_target), original_offset(-1), resume(dl_target->resume)
    {
    }

    inline Target(DownloadTarget *dl_target, const std::vector<Mirror *> &mirrors)
        : state(DownloadState::WAITING), target(dl_target), original_offset(-1), resume(dl_target->resume), mirrors(mirrors)
    {
    }

    inline ~Target()
    {
        reset();
    }

    template <class T>
    inline void setopt(CURLoption opt, const T &val)
    {
        CURLcode ok;
        if constexpr (std::is_same<T, std::string>())
        {
            ok = curl_easy_setopt(curl_handle, opt, val.c_str());
        }
        else
        {
            ok = curl_easy_setopt(curl_handle, opt, val);
        }
        if (ok != CURLE_OK)
        {
            throw curl_error(fmt::format("curl: curl_easy_setopt failed {}", curl_easy_strerror(ok)));
        }
    }

    inline void add_header(const std::string &header)
    {
        curl_rqheaders = curl_slist_append(curl_rqheaders, header.c_str());
        if (!curl_rqheaders)
        {
            throw std::bad_alloc();
        }
    }

    inline CbReturnCode call_endcallback(TransferStatus status)
    {
        EndCb end_cb = override_endcb ? override_endcb : target->endcb;
        void *cb_data = override_endcb ? override_endcb_data : target->cbdata;

        if (end_cb)
        {
            // TODO fill in message?!
            std::string message = "";
            CbReturnCode rc = end_cb(status,
                                     message, 
                                     cb_data);

            if (rc == CbReturnCode::ERROR)
            {
                cb_return_code = CbReturnCode::ERROR;
                pfdebug("End-Callback returned an error");
            }
            return rc;
        }
        return CbReturnCode::OK;
    }

    inline CURL *finalize_handle()
    {
        setopt(CURLOPT_HTTPHEADER, curl_rqheaders);
        return curl_handle;
    }

    bool truncate_transfer_file();
    std::shared_ptr<std::ofstream> open_target_file();

    CURL *handle() const;
    bool perform();
    void reset();

    DownloadTarget *target;
    fs::path out_file;
    std::string url_stub;

    bool resume = false;
    std::size_t resume_count = 0;
    std::ptrdiff_t original_offset;

    // internal stuff
    std::size_t retries;

    DownloadState state;

    // mirror list (or should we have a failure callback)
    Mirror *mirror;
    std::vector<Mirror *> mirrors;
    std::set<Mirror *> tried_mirrors;
    Mirror *used_mirror;

    HeaderCbState headercb_state;
    std::string headercb_interrupt_reason;
    std::size_t writecb_received;
    bool writecb_required_range_written;

    char errorbuffer[CURL_ERROR_SIZE];

    EndCb override_endcb = nullptr;
    void *override_endcb_data = nullptr;

    CbReturnCode cb_return_code;

    CURL *curl_handle = nullptr;
    curl_slist *curl_rqheaders = nullptr;
    Protocol protocol;

    bool range_fail = false;
    ZckState zck_state;
    FILE *f = nullptr;
};
