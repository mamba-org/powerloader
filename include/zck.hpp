#pragma once

#include <string>

#include "enums.hpp"
#include "target.hpp"

extern "C"
{
#include <fcntl.h>
#include <unistd.h>
#include <zck.h>
}

namespace powerloader
{
    class zchunk_error : public std::runtime_error
    {
    public:
        inline zchunk_error(const std::string& what = "zchunk error")
            : std::runtime_error(what)
        {
        }
    };

    zck_hash zck_hash_from_checksum(ChecksumType checksum_type);
    ChecksumType checksum_type_from_zck_hash(zck_hash hash_type);

    zckCtx* init_zck_read(const std::unique_ptr<Checksum>& chksum,
                          ptrdiff_t zck_header_size,
                          int fd);

    zckCtx* zck_init_read_base(const std::unique_ptr<Checksum>& chksum,
                               std::ptrdiff_t zck_header_size,
                               int fd);

    bool zck_valid_header_base(const std::unique_ptr<Checksum>& chksum,
                               std::ptrdiff_t zck_header_size,
                               int fd);

    zckCtx* zck_init_read(DownloadTarget* target, int fd);
    zckCtx* zck_init_read(Target* target);

    bool zck_valid_header(DownloadTarget* target, int fd);
    bool zck_valid_header(Target* target);

    bool zck_clear_header(Target* target);
    bool zck_read_lead(Target* target);
    std::vector<fs::path> get_recursive_files(fs::path dir, const std::string& suffix);

    // TODO replace...
    int lr_copy_content(int source, int dest);

    bool find_local_zck_header(Target* target);

    bool prep_zck_header(Target* target);

    bool find_local_zck_chunks(Target* target);

    bool prepare_zck_body(Target* target);
    bool check_zck(Target* target);

    bool zck_extract(const fs::path& source, const fs::path& dst, bool validate);
}
