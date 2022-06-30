#ifndef PL_TARGET_HPP
#define PL_TARGET_HPP

#include <filesystem>
#include <spdlog/spdlog.h>
#include <fstream>
#include <set>

#include "curl.hpp"
#include "download_target.hpp"
#include "enums.hpp"
#include "mirror.hpp"
#include "utils.hpp"

namespace powerloader
{
    namespace fs = std::filesystem;

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

        Target(const Context& ctx,
               std::shared_ptr<DownloadTarget> dl_target,
               std::vector<std::shared_ptr<Mirror>> mirrors = {});

        ~Target();

        bool zck_running() const;

        CbReturnCode call_endcallback(TransferStatus status);
        void reset_file(TransferStatus status);

        static int progress_callback(Target* ptr,
                                     curl_off_t total_to_download,
                                     curl_off_t now_downloaded,
                                     curl_off_t total_to_upload,
                                     curl_off_t now_uploaded);

        bool truncate_transfer_file();

        void open_target_file();

        void reset();

        bool check_filesize();
        bool check_checksums();

        std::shared_ptr<DownloadTarget> target;
        fs::path temp_file;
        std::string url_stub;

        bool resume = false;
        std::size_t resume_count = 0;
        std::ptrdiff_t original_offset;

        // internal stuff
        std::size_t retries = 0;

        DownloadState state = DownloadState::kWAITING;

        // mirror list (or should we have a failure callback)
        std::shared_ptr<Mirror> mirror = nullptr;
        std::vector<std::shared_ptr<Mirror>> mirrors;
        std::set<std::shared_ptr<Mirror>> tried_mirrors;
        std::shared_ptr<Mirror> used_mirror = nullptr;

        HeaderCbState headercb_state;
        std::string headercb_interrupt_reason;
        std::size_t writecb_received;
        bool writecb_required_range_written;

        char errorbuffer[CURL_ERROR_SIZE] = {};

        using end_callback = DownloadTarget::end_callback;
        end_callback override_endcb;
        void* override_endcb_data = nullptr;

        CbReturnCode cb_return_code;

        std::unique_ptr<CURLHandle> curl_handle;
        Protocol protocol;

        bool range_fail = false;
        ZckState zck_state;

        const Context& ctx;
    };
}

#endif
