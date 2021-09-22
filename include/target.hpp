#pragma once

#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <set>

namespace fs = std::filesystem;

#include "curl.hpp"
#include "download_target.hpp"
#include "enums.hpp"
#include "mirror.hpp"
#include "utils.hpp"

class Target
{
public:
    /** Header callback for CURL handles.
     * It parses HTTP and FTP headers and try to find length of the content
     * (file size of the target). If the size is different then the expected
     * size, then the transfer is interrupted.
     * This callback is used only if the expected size is specified.
     */
    static std::size_t header_callback(char* buffer,
                                       std::size_t size,
                                       std::size_t nitems,
                                       Target* self);
    static std::size_t write_callback(char* buffer,
                                      std::size_t size,
                                      std::size_t nitems,
                                      Target* self);

    inline Target(DownloadTarget* dl_target)
        : state(DownloadState::WAITING)
        , target(dl_target)
        , original_offset(-1)
        , resume(dl_target->resume)
    {
    }

    inline Target(DownloadTarget* dl_target, const std::vector<Mirror*>& mirrors)
        : state(DownloadState::WAITING)
        , target(dl_target)
        , original_offset(-1)
        , resume(dl_target->resume)
        , mirrors(mirrors)
    {
    }

    inline ~Target()
    {
        reset();
    }

    inline CbReturnCode call_endcallback(TransferStatus status)
    {
        EndCb end_cb = override_endcb ? override_endcb : target->endcb;
        void* cb_data = override_endcb ? override_endcb_data : target->cbdata;

        if (end_cb)
        {
            // TODO fill in message?!
            std::string message = "";
            CbReturnCode rc = end_cb(status, message, cb_data);

            if (rc == CbReturnCode::ERROR)
            {
                cb_return_code = CbReturnCode::ERROR;
                pfdebug("End-Callback returned an error");
            }
            return rc;
        }
        return CbReturnCode::OK;
    }

    static int progress_callback(Target* ptr,
                                 curl_off_t total_to_download,
                                 curl_off_t now_downloaded,
                                 curl_off_t total_to_upload,
                                 curl_off_t now_uploaded);

    bool truncate_transfer_file();
    std::shared_ptr<std::ofstream> open_target_file();

    void reset();

    DownloadTarget* target;
    fs::path out_file;
    std::string url_stub;

    bool resume = false;
    std::size_t resume_count = 0;
    std::ptrdiff_t original_offset;

    // internal stuff
    std::size_t retries;

    DownloadState state = DownloadState::WAITING;

    // mirror list (or should we have a failure callback)
    Mirror* mirror;
    std::vector<Mirror*> mirrors;
    std::set<Mirror*> tried_mirrors;
    Mirror* used_mirror;

    HeaderCbState headercb_state;
    std::string headercb_interrupt_reason;
    std::size_t writecb_received;
    bool writecb_required_range_written;

    char errorbuffer[CURL_ERROR_SIZE];

    EndCb override_endcb = nullptr;
    void* override_endcb_data = nullptr;

    CbReturnCode cb_return_code;

    // CURL *curl_handle = nullptr;
    std::unique_ptr<CURLHandle> curl_handle;
    Protocol protocol;

    bool range_fail = false;
    ZckState zck_state;
    FILE* f = nullptr;
};
