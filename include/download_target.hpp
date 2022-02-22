#pragma once

#include <string>

#include "enums.hpp"
#include "url.hpp"
#include "mirror.hpp"
#include "fileio.hpp"
#include "errors.hpp"

#ifdef WITH_ZCHUNK
extern "C"
{
#include <zck.h>
}
#endif

namespace powerloader
{
    /** Called when a transfer is done (use transfer status to check
     * if successful or failed).
     * @param clientp           Pointer to user data.
     * @param status            Transfer status
     * @param msg               Error message or NULL.
     * @return                  See LrCbReturnCode codes
     */
    typedef CbReturnCode (*EndCb)(TransferStatus status, const std::string& msg, void* clientp);

    class DownloadTarget
    {
    public:
        inline DownloadTarget(const std::string& path,
                              const std::string& base_url,
                              const fs::path& fn)
            : path(path)
            , fn(fn)
            , base_url(base_url)
            , is_zchunk(ends_with(path, ".zck"))
        {
            if (path.find("://") != std::string::npos)
            {
                complete_url = path;
            }
            else if (base_url.find("://") != std::string::npos)
            {
                complete_url = join_url(base_url, path);
            }

#if WITH_ZCHUNK
            if (is_zchunk)
            {
                zck_cache_file = fn;
            }
#endif
        }

        inline bool has_complete_url()
        {
            return !complete_url.empty();
        }

        inline bool validate_checksum(const fs::path& path)
        {
            if (checksums.empty())
                return false;

            auto findchecksum = [&](const ChecksumType& t) -> Checksum*
            {
                for (auto& cs : checksums)
                {
                    if (cs.type == t)
                        return &cs;
                }
                return nullptr;
            };

            Checksum* cs;
            if ((cs = findchecksum(ChecksumType::kSHA256)))
            {
                auto sum = sha256sum(path);

                if (sum != cs->checksum)
                {
                    spdlog::error("SHA256 sum of downloaded file is wrong.\nIs {}. Should be {}",
                                  sum,
                                  cs->checksum);
                    return false;
                }
                return true;
            }
            else if ((cs = findchecksum(ChecksumType::kSHA1)))
            {
                spdlog::error("Checking SHA1 sum not implemented!");
                return false;
            }
            else if ((cs = findchecksum(ChecksumType::kMD5)))
            {
                spdlog::info("Checking MD5 sum");
                auto sum = md5sum(path);
                if (sum != cs->checksum)
                {
                    spdlog::error("MD5 sum of downloaded file is wrong.\nIs {}. Should be {}",
                                  sum,
                                  cs->checksum);
                    return false;
                }
                return true;
            }
            return false;
        }

        bool already_downloaded()
        {
            if (checksums.empty())
                return false;
            return fs::exists(fn) && validate_checksum(fn);
        }

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

        std::function<int(curl_off_t, curl_off_t)> progress_callback;


        EndCb endcb = nullptr;
        void* cbdata = nullptr;

        // these are available checksums for the entire file
        std::vector<Checksum> checksums;

        std::shared_ptr<Mirror> used_mirror;
        std::string effective_url;
        std::unique_ptr<DownloaderError> error;

        inline void set_error(const DownloaderError& err)
        {
            error = std::make_unique<DownloaderError>(err);
        }

#ifdef WITH_ZCHUNK
        // Zchunk download context
        zckDL* zck_dl = nullptr;

        // Zchunk header size
        std::ptrdiff_t zck_header_size = -1;
        std::unique_ptr<Checksum> zck_header_checksum;

        fs::path zck_cache_file;

        // Total to download in zchunk file
        double total_to_download;

        // Amount already downloaded in zchunk file
        double downloaded;
#endif /* WITH_ZCHUNK */
    };

}
