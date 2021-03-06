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

    struct zck_target;

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

        DownloadTarget(const std::string& path,
                       const std::string& base_url,
                       const fs::path& filename);
        ~DownloadTarget();

        DownloadTarget(const DownloadTarget&) = delete;
        DownloadTarget& operator=(const DownloadTarget&) = delete;
        DownloadTarget(DownloadTarget&&) = delete;
        DownloadTarget& operator=(DownloadTarget&&) = delete;

        void set_cache_options(const CacheControl& cache_control);
        void add_handle_options(CURLHandle& handle);

        bool has_complete_url() const;
        bool validate_checksum(const fs::path& path);
        bool already_downloaded();

        void set_error(const DownloaderError& err);

        bool resume() const noexcept
        {
            return m_resume;
        }

        void set_resume(bool new_value) noexcept
        {
            m_resume = new_value;
        }

        bool no_cache() const noexcept
        {
            return m_no_cache;
        }

        const std::string& base_url() const noexcept
        {
            return m_base_url;
        }

        void clear_base_url()
        {
            m_base_url.clear();
        }


        const std::string& complete_url() const noexcept
        {
            return m_complete_url;
        }

        const std::string& path() const noexcept
        {
            return m_path;
        }

        const std::filesystem::path& filename() const noexcept
        {
            return m_filename;
        }

        std::size_t byterange_start() const noexcept
        {
            return m_byterange_start;
        }

        std::size_t byterange_end() const noexcept
        {
            return m_byterange_end;
        }

        std::ptrdiff_t expected_size() const noexcept
        {
            return m_expected_size;
        }

        // TOOD: check SOC (why is this modifed outside)
        void set_expected_size(std::ptrdiff_t value)
        {
            // TODO: add checks?
            m_expected_size = value;
        }

        std::ptrdiff_t orig_size() const noexcept
        {
            return m_orig_size;
        }

        const std::string& range() const noexcept
        {
            return m_range;
        }

        // TODO: fix SOC with zchunk's code modifying this outside
        std::string& range() noexcept
        {
            return m_range;
        }

        bool is_zchunck() const noexcept
        {
            return m_is_zchunk;
        }

        const zck_target& zck() const
        {
            if (!is_zchunck())  // TODO: REVIEW: should this be an assert?
                throw std::invalid_argument("attempted to access zchunk data but there is none");
            return *m_p_zck;
        }

        // TODO: ownership/access issue: mostly modified outside
        zck_target& zck()
        {
            if (!is_zchunck())  // TODO: REVIEW: should this be an assert?
                throw std::invalid_argument("attempted to access zchunk data but there is none");
            return *m_p_zck;
        }

        // TODO: rewrite outfile's ownership handling and processing to avoid sharing it with other
        // types's implementations
        void set_outfile(std::unique_ptr<FileIO> new_outfile)
        {
            m_outfile = std::move(new_outfile);
        }

        // TODO: rewrite outfile's ownership handling and processing to avoid sharing it with other
        // types's implementations
        std::unique_ptr<FileIO>& outfile() noexcept
        {
            return m_outfile;
        }

        const progress_callback_t& progress_callback() const
        {
            return m_progress_callback;
        }

        progress_callback_t set_progress_callback(progress_callback_t callback)
        {
            std::exchange(callback, m_progress_callback);
            return callback;
        }

        const end_callback_t& end_callback() const
        {
            return m_end_callback;
        }

        end_callback_t set_end_callback(end_callback_t callback)
        {
            std::exchange(callback, m_end_callback);
            return callback;
        }

        std::shared_ptr<Mirror> set_mirror_to_use(std::shared_ptr<Mirror> mirror)
        {
            std::exchange(mirror, m_used_mirror);
            return mirror;
        }

        std::shared_ptr<Mirror> used_mirror() const noexcept
        {
            return m_used_mirror;
        }

        const std::string& effective_url() const noexcept
        {
            return m_effective_url;
        }

        void set_effective_url(const std::string& new_url)
        {
            // TODO: add some checks here
            m_effective_url = new_url;
        }

        const std::vector<Checksum>& checksums() const
        {
            return m_checksums;
        }

        // TODO: consider making the whole set of checksums one value set when needed.
        void add_checksum(Checksum value)
        {
            m_checksums.push_back(value);
        }

    private:
        bool m_is_zchunk = false;
        bool m_resume = true;
        bool m_no_cache = false;

        std::string m_complete_url;
        std::string m_path;
        std::string m_base_url;
        std::unique_ptr<FileIO> m_outfile;

        fs::path m_filename;

        std::size_t m_byterange_start = 0;
        std::size_t m_byterange_end = 0;
        std::string m_range;
        std::ptrdiff_t m_expected_size = 0;
        std::ptrdiff_t m_orig_size = 0;

        progress_callback_t m_progress_callback;
        end_callback_t m_end_callback;

        // these are available checksums for the entire file
        std::vector<Checksum> m_checksums;

        std::shared_ptr<Mirror> m_used_mirror;
        std::string m_effective_url;
        std::unique_ptr<DownloaderError> m_error;  // TODO: check if it's used somewhere outside
                                                   // this project, or use in tests at least.

        std::unique_ptr<zck_target> m_p_zck;

        CacheControl m_cache_control;
    };

}

#endif
