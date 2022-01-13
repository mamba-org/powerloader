#pragma once

#include <string>

#include "enums.hpp"
#include "url.hpp"
#include "mirror.hpp"
#include "fileio.hpp"

#ifdef WITH_ZCHUNK
extern "C"
{
#include <zck.h>
}
#endif

namespace powerloader
{
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

        bool has_complete_url()
        {
            return !complete_url.empty();
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

        Mirror* used_mirror;

        std::function<int(curl_off_t, curl_off_t)> progress_callback;

        EndCb endcb = nullptr;
        void* cbdata = nullptr;

        // these are available checksums for the entire file
        std::vector<Checksum> checksums;

        // error code

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
