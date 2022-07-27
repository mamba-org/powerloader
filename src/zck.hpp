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

    POWERLOADER_API zck_hash zck_hash_from_checksum(ChecksumType checksum_type);
    POWERLOADER_API ChecksumType checksum_type_from_zck_hash(zck_hash hash_type);

    POWERLOADER_API
    zckCtx* init_zck_read(const Checksum& chksum, ptrdiff_t zck_header_size, int fd);

    POWERLOADER_API
    zckCtx* zck_init_read_base(const Checksum& chksum, std::ptrdiff_t zck_header_size, int fd);

    POWERLOADER_API
    bool zck_valid_header_base(const Checksum& chksum, std::ptrdiff_t zck_header_size, int fd);

    POWERLOADER_API zckCtx* zck_init_read(const DownloadTarget& target, int fd);
    POWERLOADER_API zckCtx* zck_init_read(const Target& target);

    POWERLOADER_API bool zck_valid_header(const DownloadTarget& target, int fd);
    POWERLOADER_API bool zck_valid_header(const Target& target);

    POWERLOADER_API bool zck_clear_header(Target& target);
    POWERLOADER_API bool zck_read_lead(Target& target);
    POWERLOADER_API std::vector<fs::path> get_recursive_files(const fs::path& dir,
                                                              const std::string& suffix);

    // TODO replace...
    POWERLOADER_API int lr_copy_content(int source, int dest);

    POWERLOADER_API bool find_local_zck_header(Target& target);

    POWERLOADER_API bool prep_zck_header(Target& target);

    POWERLOADER_API bool find_local_zck_chunks(Target& target);

    POWERLOADER_API bool prepare_zck_body(Target& target);
    POWERLOADER_API bool check_zck(Target& target);

    POWERLOADER_API bool zck_extract(const fs::path& source, const fs::path& dst, bool validate);
}

#endif
