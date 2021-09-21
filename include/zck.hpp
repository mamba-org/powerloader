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

class zchunk_error : public std::runtime_error
{
public:
    inline zchunk_error(const std::string& what = "zchunk error")
        : std::runtime_error(what)
    {
    }
};

zck_hash
zck_hash_from_checksum(ChecksumType checksum_type);

zckCtx*
init_zck_read(const char* checksum, ChecksumType checksum_type, ptrdiff_t zck_header_size, int fd);

zckCtx*
zck_init_read_base(const char* checksum,
                   ChecksumType checksum_type,
                   std::ptrdiff_t zck_header_size,
                   int fd);

bool
zck_valid_header_base(const char* checksum,
                      ChecksumType checksum_type,
                      std::ptrdiff_t zck_header_size,
                      int fd);

zckCtx*
zck_init_read(DownloadTarget* target, int fd);

bool
zck_valid_header(DownloadTarget* target, int fd);

bool
zck_clear_header(Target* target);

std::vector<fs::path>
get_recursive_files(fs::path dir, const std::string& suffix);

// TODO replace...
int
lr_copy_content(int source, int dest);

bool
find_local_zck_header(Target* target);

bool
prep_zck_header(Target* target);

bool
find_local_zck_chunks(Target* target);

bool
prepare_zck_body(Target* target);
bool
check_zck(Target* target);
