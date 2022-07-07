#ifndef POWERLOADER_DOWNLOAD_TARGET_HPP
#define POWERLOADER_DOWNLOAD_TARGET_HPP

#include <string>

#include <powerloader/export.hpp>
#include <powerloader/enums.hpp>
#include <powerloader/url.hpp>
#include <powerloader/curl.hpp>
#include <powerloader/mirror.hpp>
#include <powerloader/fileio.hpp>
#include <powerloader/errors.hpp>

namespace powerloader
{

    class zck_target;

    struct POWERLOADER_API CacheControl
    {
        std::string etag;
        std::string cache_control;
        std::string last_modified;
    };

    class POWERLOADER_API DownloadTarget
    {
    public:
        /** Called when a transfer is done (use transfer status to check
         * if successful or failed).
         * @param clientp           Pointer to user data.
         * @param status            Transfer status
         * @param msg               Error message or NULL.
         * @return                  See LrCbReturnCode codes
         */
        using end_callback_t = std::function<CbReturnCode(TransferStatus, const Response&)>;
        using progress_callback_t = std::function<int(curl_off_t, curl_off_t)>;

        DownloadTarget(const std::string& path, const std::string& base_url, const fs::path& fn);
        ~DownloadTarget();

        void set_cache_options(const CacheControl& cache_control);
        void add_handle_options(CURLHandle& handle);

        DownloadTarget(const DownloadTarget&) = delete;
        DownloadTarget& operator=(const DownloadTarget&) = delete;
        DownloadTarget(DownloadTarget&&) = delete;
        DownloadTarget& operator=(DownloadTarget&&) = delete;

        bool has_complete_url() const;
        bool validate_checksum(const fs::path& path);
        bool already_downloaded();

        void set_error(const DownloaderError& err);


        bool is_zchunk = false;
        bool resume = true;
        bool no_cache = false;

        std::string complete_url;
        std::string path, base_url;
        std::unique_ptr<FileIO> outfile;

        fs::path fn;

        std::size_t byterange_start = 0, byterange_end = 0;
        std::string range;
        std::ptrdiff_t expected_size = 0;
        std::ptrdiff_t orig_size = 0;

        progress_callback_t progress_callback;
        end_callback_t end_callback;

        // these are available checksums for the entire file
        std::vector<Checksum> checksums;

        std::shared_ptr<Mirror> used_mirror;
        std::string effective_url;
        std::unique_ptr<DownloaderError> error;

        // We cannot use a unique_ptr here because of the python bindings
        // which needs the sizeof zck_target
        zck_target* p_zck;

    private:
        CacheControl m_cache_control;
    };

}

#endif
