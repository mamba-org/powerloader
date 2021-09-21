#pragma once

#include <string>

#include "enums.hpp"
#include "mirror.hpp"

extern "C"
{
#include <zck.h>
}

class DownloadTarget
{
public:
    inline DownloadTarget(const std::string& path,
                          const std::string& base_url,
                          const std::string& fn)
        : path(path)
        , fn(fn)
        , base_url(base_url)
        , is_zchunk(ends_with(path, ".zck"))
    {
    }

    bool is_zchunk = false;
    bool resume = true;
    bool no_cache = false;

    std::string fn, path, base_url;
    std::shared_ptr<std::ofstream> fd;

    std::size_t byterange_start = 0, byterange_end = 0;
    std::string range;
    std::ptrdiff_t expected_size = 0;
    std::ptrdiff_t orig_size = 0;

    Mirror* used_mirror;

    EndCb endcb = nullptr;
    void* cbdata = nullptr;

    std::vector<Checksum> checksums;

    // used mirror
    // error code

    // #ifdef WITH_ZCHUNK
    zckDL* zck_dl = nullptr; /*!<
        Zchunk download context */

    std::ptrdiff_t zck_header_size; /*!<
        Zchunk header size */

    double total_to_download; /*!<
        Total to download in zchunk file */

    double downloaded; /*!<
        Amount already downloaded in zchunk file */
                       // #endif /* WITH_ZCHUNK */
};
