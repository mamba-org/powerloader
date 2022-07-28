#ifndef POWERLOADER_ZCK_HPP
#define POWERLOADER_ZCK_HPP

#include <string>

#include <powerloader/export.hpp>
#include <powerloader/enums.hpp>
#include <powerloader/target.hpp>

extern "C"
{
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <zck.h>
}

namespace powerloader
{
    struct zchunk_error : public std::runtime_error
    {
        zchunk_error(const std::string& what = "zchunk error")
            : std::runtime_error(what)
        {
        }
    };

    struct zck_target
    {
        // Zchunk download context
        zckDL* zck_dl = nullptr;

        // Zchunk header size
        std::ptrdiff_t zck_header_size = -1;
        std::unique_ptr<Checksum> zck_header_checksum;

        fs::path zck_cache_file;

        // Total to download in zchunk file
        std::uint64_t total_to_download = 0;

        // Amount already downloaded in zchunk file
        std::uint64_t downloaded = 0;
    };

    bool zck_read_lead(Target& target);

    zckCtx* zck_init_read(const DownloadTarget& target, int fd);
    zckCtx* zck_init_read(const Target& target);

    bool zck_valid_header(const DownloadTarget& target, int fd);
    bool zck_valid_header(const Target& target);

    bool check_zck(Target& target);

    bool zck_extract(const fs::path& source, const fs::path& dst, bool validate);

}

#endif
