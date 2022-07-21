#ifndef POWERLOADER_TARGET_HPP
#define POWERLOADER_TARGET_HPP

#include <filesystem>
#include <fstream>
#include <set>
#include <spdlog/spdlog.h>

#include <powerloader/export.hpp>
#include <powerloader/curl.hpp>
#include <powerloader/download_target.hpp>
#include <powerloader/enums.hpp>
#include <powerloader/mirror.hpp>
#include <powerloader/utils.hpp>

namespace powerloader
{
    namespace fs = std::filesystem;

    class POWERLOADER_API Target
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

        CbReturnCode call_end_callback(TransferStatus status);


        // Mark the target as failed and returns the value returned by the end callback
        CbReturnCode set_failed(DownloaderError error);

        // Completes transfer that finished successfully.
        void finalize_transfer(const std::string& effective_url);

        bool set_retrying();

        // Changes the state of this target IFF the downloaded file already exists (however it was
        // produced).
        void check_if_already_finished();

        // Forces max speed (provided by context) for already prepared Target.
        // Requirement: `prepare_for_transfer()` must have been called successfully before calling
        // this function.
        void set_to_max_speed();

        void reset();
        void reset_response();

        void prepare_for_transfer(CURLM* multi_handle,
                                  const std::string& full_url,
                                  Protocol protocol);

        void flush_target_file();

        tl::expected<void, DownloaderError> finish_transfer(const std::string& effective_url);

        void complete_mirror_usage(bool was_success,
                                   const tl::expected<void, DownloaderError>& result);

        bool can_retry_transfer_with_fewer_connections() const;
        void lower_mirror_parallel_connections();

        const DownloadTarget& target() const noexcept
        {
            assert(m_target);
            return *m_target;
        }

        // TODO: don't allow external code from manipulating this internal target.
        DownloadTarget& target() noexcept
        {
            assert(m_target);
            return *m_target;
        }

        // TODO: don't expose ownership
        const std::shared_ptr<Mirror>& mirror() const noexcept
        {
            assert(m_mirror);
            return m_mirror;
        }

        // TODO: don't expose ownership
        std::shared_ptr<Mirror>& mirror() noexcept
        {
            assert(m_mirror);
            return m_mirror;
        }

        void change_mirror(std::shared_ptr<Mirror> mirror);

        const fs::path temp_file() const noexcept
        {
            return m_temp_file;
        }

        bool writecb_required_range_written() const noexcept
        {
            return m_writecb_required_range_written;
        }

        HeaderCbState headercb_state() const noexcept
        {
            return m_headercb_state;
        }

        const std::string& headercb_interrupt_reason() const noexcept
        {
            return m_headercb_interrupt_reason;
        }

        int range_fail() const noexcept
        {
            return m_range_fail;
        }

        void reset_range_fail() noexcept
        {
            m_range_fail = false;
        }

        std::size_t retries() const noexcept
        {
            return m_retries;
        }

        DownloadState state() const noexcept
        {
            return m_state;
        }

        ZckState zck_state() const noexcept
        {
            return m_zck_state;
        }

        // TODO: refactor to avoid state being changed directly from outisde.
        void set_zck_state(ZckState new_state) noexcept
        {
            m_zck_state = new_state;
        }

        const auto& errorbuffer() const noexcept
        {
            return m_errorbuffer;
        }

        const auto& mirrors() const noexcept
        {
            return m_mirrors;
        }

        const auto& tried_mirrors() const noexcept
        {
            return m_tried_mirrors;
        }

        CURL* curl() const noexcept
        {
            return m_curl_handle.get();
        }

    private:
        std::shared_ptr<DownloadTarget> m_target;
        fs::path m_temp_file;
        std::string m_url_stub;

        bool m_resume = false;
        std::size_t m_resume_count = 0;
        std::ptrdiff_t m_original_offset;

        // internal stuff
        std::size_t m_retries = 0;

        DownloadState m_state = DownloadState::kWAITING;

        // mirror list (or should we have a failure callback)
        std::shared_ptr<Mirror> m_mirror;
        std::vector<std::shared_ptr<Mirror>> m_mirrors;
        std::set<std::shared_ptr<Mirror>> m_tried_mirrors;
        std::shared_ptr<Mirror> m_used_mirror;

        HeaderCbState m_headercb_state;
        std::string m_headercb_interrupt_reason;
        std::size_t m_writecb_received;
        bool m_writecb_required_range_written;

        char m_errorbuffer[CURL_ERROR_SIZE] = {};

        std::unique_ptr<CURLHandle> m_curl_handle;
        Protocol m_protocol;

        Response m_response;

        bool m_range_fail = false;
        ZckState m_zck_state;

        const Context& m_ctx;

        bool zck_running() const;

        void reset_file(TransferStatus status);

        static int progress_callback(Target* ptr,
                                     curl_off_t total_to_download,
                                     curl_off_t now_downloaded,
                                     curl_off_t total_to_upload,
                                     curl_off_t now_uploaded);

        bool truncate_transfer_file();

        void open_target_file();

        bool check_filesize();
        bool check_checksums();

        friend std::size_t zckwritecb(char* buffer, size_t size, size_t nitems, Target* self);
        friend std::size_t zckheadercb(char* buffer,
                                       std::size_t size,
                                       std::size_t nitems,
                                       Target* self);
    };
}

#endif
