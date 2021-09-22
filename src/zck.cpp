#include "zck.hpp"
#include "target.hpp"

#ifdef WITH_ZCHUNK

zck_hash
zck_hash_from_checksum(ChecksumType checksum_type)
{
    switch (checksum_type)
    {
        case ChecksumType::SHA1:
            return ZCK_HASH_SHA1;
        case ChecksumType::SHA256:
            return ZCK_HASH_SHA256;
        default:
            return ZCK_HASH_UNKNOWN;
    }
}

zckCtx*
init_zck_read(const char* checksum, ChecksumType checksum_type, ptrdiff_t zck_header_size, int fd)
{
    // assert(!err || *err == NULL);

    zckCtx* zck = zck_create();
    if (!zck_init_adv_read(zck, fd))
    {
        throw zchunk_error("Unable to initialize zchunk file for reading");
    }
    std::cout << "Strlen: " << strlen(checksum) << std::endl;
    std::cout << "Hash setting to " << checksum << std::endl;
    zck_hash ct = zck_hash_from_checksum(checksum_type);
    if (ct == ZCK_HASH_UNKNOWN)
    {
        throw zchunk_error(fmt::format("Zchunk doesn't support checksum type {}", checksum_type));
        // free(zck);
    }
    if (!zck_set_ioption(zck, ZCK_VAL_HEADER_HASH_TYPE, ct))
    {
        throw zchunk_error("Error setting validation checksum type");
        // free(zck);
    }
    if (!zck_set_ioption(zck, ZCK_VAL_HEADER_LENGTH, zck_header_size))
    {
        throw zchunk_error("Error setting header size");
        // free(zck);
    }
    if (!zck_set_soption(zck, ZCK_VAL_HEADER_DIGEST, checksum, strlen(checksum)))
    {
        throw zchunk_error(fmt::format("Unable to set validation checksum: {}", checksum));
        // free(zck);
        // return NULL;
    }
    std::cout << "Successful init zck read" << std::endl;
    return zck;
}

zckCtx*
zck_init_read_base(const char* checksum,
                   ChecksumType checksum_type,
                   std::ptrdiff_t zck_header_size,
                   int fd)
{
    lseek(fd, 0, SEEK_SET);
    zckCtx* zck = init_zck_read(checksum, checksum_type, zck_header_size, fd);

    std::cout << "Reading base " << checksum << " " << zck_header_size << std::endl;

    if (zck == nullptr)
        return nullptr;

    if (!zck_read_lead(zck))
    {
        throw zchunk_error("Unable to read zchunk lead II");
        // zck_free(&zck);
        // return NULL;
    }
    if (!zck_read_header(zck))
    {
        throw zchunk_error("Unable to read zchunk header III");
        // zck_free(&zck);
        // return NULL;
    }
    return zck;
}

bool
zck_valid_header_base(const char* checksum,
                      ChecksumType checksum_type,
                      std::ptrdiff_t zck_header_size,
                      int fd)
{
    // lseek(fd, 0L, SEEK_END);
    // std::cout << "File header size: " << ltell(fd) << std::endl;
    lseek(fd, 0, SEEK_SET);
    zckCtx* zck = init_zck_read(checksum, checksum_type, zck_header_size, fd);
    if (zck == nullptr)
        return false;

    if (!zck_validate_lead(zck))
    {
        throw zchunk_error(fmt::format("Unable to read zchunk lead"));
        // zck_free(&zck);
        // return FALSE;
    }
    zck_free(&zck);
    return true;
}

zckCtx*
zck_init_read(DownloadTarget* target, int fd)
{
    zckCtx* zck = nullptr;
    bool found = false;
    for (auto& chksum : target->checksums)
    {
        pfdebug("Checking checksum: {}: {}", chksum.type, "lll");
        try
        {
            std::cout << "CHKSZ: " << chksum.checksum.size() << std::endl;
            zck = zck_init_read_base(
                chksum.checksum.data(), chksum.type, target->zck_header_size, fd);
        }
        catch (const std::exception& e)
        {
            pfdebug("Didnt find matching header...");
            continue;
        }
        found = true;
        break;
    }
    if (!found)
    {
        throw zchunk_error("Zchunk header checksum didn't match expected checksum");
    }
    return zck;
}

bool
zck_valid_header(DownloadTarget* target, int fd)
{
    for (auto& chksum : target->checksums)
    {
        std::cout << "CHKSZ: " << chksum.checksum.size() << std::endl;

        if (zck_valid_header_base(chksum.checksum.data(), chksum.type, target->zck_header_size, fd))
        {
            return true;
        }
    }
    throw zchunk_error(fmt::format("{}'s zchunk header doesn't match", target->path));
}

bool
zck_clear_header(Target* target)
{
    // assert(target && target->f && target->target && target->target->path);

    // TODO

    int fd = fileno(target->f);
    lseek(fd, 0, SEEK_END);
    if (ftruncate(fd, 0) < 0)
    {
        std::cout << "TRUNCATE went wrong!!!" << std::endl;
        // g_set_error(err, LR_DOWNLOADER_ERROR, LRE_IO,
        //             "Unable to truncate %s", target->target->path);
        return false;
    }
    else
    {
        return true;
    }
    return true;
}

std::vector<fs::path>
get_recursive_files(fs::path dir, const std::string& suffix)
{
    std::vector<fs::path> res;
    for (auto& p : fs::recursive_directory_iterator(dir))
    {
        if (ends_with(p.path().filename().string(), suffix))
        {
            res.push_back(p.path());
        }
    }
    return res;
}

// TODO replace...
int
lr_copy_content(int source, int dest)
{
    const int bufsize = 2048;
    char buf[bufsize];
    ssize_t size;

    lseek(source, 0, SEEK_SET);
    lseek(dest, 0, SEEK_SET);

    while ((size = read(source, buf, bufsize)) > 0)
        if (write(dest, buf, size) == -1)
            return -1;

    return (size < 0) ? -1 : 0;
}

bool
find_local_zck_header(Target* target)
{
    zckCtx* zck = nullptr;
    bool found = false;
    int fd = fileno(target->f);

    // if (target->target->handle->cachedir)
    if (true)
    {
        pfdebug("Cache directory: {}", CACHEDIR);
        auto filelist = get_recursive_files(CACHEDIR, ".zck");

        fs::path destdir = CACHEDIR;
        fs::path dest = destdir / target->target->path;

        for (const auto& file : filelist)
        {
            if (dest == file)
            {
                continue;
            }

            int chk_fd = open(file.c_str(), O_RDONLY);
            if (chk_fd < 0)
            {
                // pfdebug("WARNING: Unable to open {}: {}", cf, g_strerror(errno));
                pfdebug("WARNING: Unable to open {}", file.string());
                continue;
            }
            bool valid_header = false;
            try
            {
                valid_header = zck_valid_header(target->target, chk_fd);
            }
            catch (zchunk_error& e)
            {
                std::cout << "No valid header " << e.what() << std::endl;
            };

            if (valid_header)
            {
                pfdebug("zchunk: Found file with same header at ", file.string());
                if (lr_copy_content(chk_fd, fd) == 0
                    && ftruncate(fd, lseek(chk_fd, 0, SEEK_END)) >= 0 && lseek(fd, 0, SEEK_SET) == 0
                    && (zck = zck_init_read(target->target, chk_fd)))
                {
                    found = true;
                    break;
                }
                else
                {
                    pfdebug("Error copying file");
                    // g_clear_error(&tmp_err);
                }
            }
            close(chk_fd);
        }
    }
    else
    {
        pfdebug("No cache directory set.");
    }

    if (found)
    {
        zckCtx* old_zck = zck_dl_get_zck(target->target->zck_dl);
        zck_free(&old_zck);
        if (!zck_dl_set_zck(target->target->zck_dl, zck))
        {
            throw zchunk_error(fmt::format("Unable to setup zchunk download context for {}",
                                           target->target->path));
        }
        target->zck_state = ZckState::BODY_CK;
        return true;
    }
    target->zck_state = ZckState::HEADER;
    return true;
}

bool
prep_zck_header(Target* target)
{
    zckCtx* zck = nullptr;
    int fd = fileno(target->f);

    bool valid_header = false;
    try
    {
        valid_header = zck_valid_header(target->target, fd);
    }
    catch (const zchunk_error& e)
    {
        std::cout << "No valid header " << e.what() << std::endl;
    }
    std::cout << "Got valid header?! " << valid_header << std::endl;
    if (valid_header)
    {
        try
        {
            zck = zck_init_read(target->target, fd);
        }
        catch (const zchunk_error& e)
        {
            pfdebug("Error reading validated header {}", e.what());
        }
        if (zck)
        {
            if (!zck_dl_set_zck(target->target->zck_dl, zck))
            {
                throw zchunk_error("Unable to setup zchunk download context");
            }
            target->zck_state = ZckState::BODY_CK;
            return true;
        }
    }

    lseek(fd, 0, SEEK_SET);
    zck = zck_create();
    if (!zck_init_adv_read(zck, fd))
    {
        throw zchunk_error(
            fmt::format("Unable to initialize zchunk file {} for reading", target->target->path));
    }

    if (target->target->zck_dl)
    {
        zckCtx* old_zck = zck_dl_get_zck(target->target->zck_dl);
        zck_free(&old_zck);
        if (!zck_dl_set_zck(target->target->zck_dl, zck))
        {
            throw zchunk_error(fmt::format("Unable to setup zchunk download context for {}",
                                           target->target->path));
        }
    }
    else
    {
        target->target->zck_dl = zck_dl_init(zck);
    }
    target->target->range = zck_get_range(0, target->target->zck_header_size - 1);
    target->target->total_to_download = target->target->zck_header_size;
    target->target->resume = false;
    target->zck_state = ZckState::HEADER;
    std::cout << "Header download prepared!" << std::endl;
    return zck_clear_header(target);
}

bool
find_local_zck_chunks(Target* target)
{
    assert(target && target->target && target->target->zck_dl);

    zckCtx* zck = zck_dl_get_zck(target->target->zck_dl);
    int fd = fileno(target->f);
    if (zck && fd != zck_get_fd(zck) && !zck_set_fd(zck, fd))
    {
        throw zchunk_error(fmt::format("Unable to set zchunk file descriptor for {}: {}",
                                       target->target->path,
                                       zck_get_error(zck)));
    }
    // if (target->target->handle->cachedir)
    if (true)
    {
        pfdebug("Cache directory: {}", CACHEDIR);
        auto filelist = get_recursive_files(CACHEDIR, ".zck");
        bool found = false;

        fs::path destdir = CACHEDIR;
        fs::path dest = destdir / target->target->path;

        for (const auto& file : filelist)
        {
            if (dest == file)
            {
                continue;
            }

            int chk_fd = open(file.c_str(), O_RDONLY);
            if (chk_fd < 0)
            {
                // pfdebug("WARNING: Unable to open {}: {}", cf, g_strerror(errno));
                pfdebug("WARNING: Unable to open {}", file.string());
                continue;
            }

            zckCtx* zck_src = zck_create();
            if (!zck_init_read(zck_src, chk_fd))
            {
                close(chk_fd);
                continue;
            }

            if (!zck_copy_chunks(zck_src, zck))
            {
                pfdebug("Error copying chunks: {}", zck_get_error(zck));
                // g_warning("Error copying chunks from %s to %s: %s", cf, uf,
                // zck_get_error(zck));
                zck_free(&zck_src);
                close(chk_fd);
                continue;
            }
            zck_free(&zck_src);
            close(chk_fd);
        }
    }
    target->target->downloaded = target->target->total_to_download;
    /* Calculate how many bytes need to be downloaded */
    for (zckChunk* idx = zck_get_first_chunk(zck); idx != NULL; idx = zck_get_next_chunk(idx))
        if (zck_get_chunk_valid(idx) != 1)
            target->target->total_to_download
                += zck_get_chunk_comp_size(idx) + 92; /* Estimate of multipart overhead */
    target->zck_state = ZckState::BODY;

    return true;
}

bool
prepare_zck_body(Target* target)
{
    zckCtx* zck = zck_dl_get_zck(target->target->zck_dl);
    int fd = fileno(target->f);
    if (zck && fd != zck_get_fd(zck) && !zck_set_fd(zck, fd))
    {
        throw zchunk_error(fmt::format("Unable to set zchunk file descriptor for {}: {}",
                                       target->target->path,
                                       zck_get_error(zck)));
    }

    zck_reset_failed_chunks(zck);
    if (zck_missing_chunks(zck) == 0)
    {
        target->zck_state = ZckState::FINISHED;
        return true;
    }

    lseek(fd, 0, SEEK_SET);

    pfdebug("Chunks that still need to be downloaded: {}", zck_missing_chunks(zck));

    zck_dl_reset(target->target->zck_dl);
    zckRange* range = zck_get_missing_range(zck, target->mirror ? target->mirror->max_ranges : -1);
    zckRange* old_range = zck_dl_get_range(target->target->zck_dl);
    if (old_range)
    {
        zck_range_free(&old_range);
    }

    if (!zck_dl_set_range(target->target->zck_dl, range))
    {
        throw zchunk_error("Unable to set range for zchunk downloader");
        return false;
    }

    target->target->range = zck_get_range_char(zck, range);
    target->target->expected_size = 1;
    target->zck_state = ZckState::BODY;

    return true;
}

bool
check_zck(Target* target)
{
    assert(target);
    assert(target->f);
    assert(target->target);

    if (target->mirror
        && (target->mirror->max_ranges == 0 || target->mirror->protocol != Protocol::HTTP))
    {
        target->zck_state = ZckState::BODY;
        target->target->expected_size = target->target->orig_size;
        target->target->range.clear();
        return true;
    }

    std::cout << "Checking zck ..." << std::endl;

    if (target->target->zck_dl == nullptr)
    {
        std::cout << "Init zck!" << std::endl;

        target->target->zck_dl = zck_dl_init(nullptr);
        if (target->target->zck_dl == nullptr)
        {
            throw zchunk_error(zck_get_error(nullptr));
        }
        target->zck_state = ZckState::HEADER_CK;
    }

    std::cout << "ZCK DL: " << target->target->zck_dl << std::endl;

    /* Reset range fail flag */
    target->range_fail = false;

    /* If we've finished, then there's no point in checking any further */
    if (target->zck_state == ZckState::FINISHED)
        return true;

    zckCtx* zck = zck_dl_get_zck(target->target->zck_dl);
    std::cout << "Got a zck dl? " << zck << std::endl;

    if (!zck)
    {
        target->zck_state = ZckState::HEADER_CK;
        pfdebug("Unable to read zchunk header: {}", target->target->path);
        if (!find_local_zck_header(target))
            return false;
    }

    zck = zck_dl_get_zck(target->target->zck_dl);
    std::cout << fmt::format("Everything fine? {}", zck_get_error(zck)) << " " << zck << " & "
              << target->target->zck_dl << std::endl;

    if (target->zck_state == ZckState::HEADER)
    {
        std::cout << "prepzckheader" << std::endl;
        if (!prep_zck_header(target))
            return false;

        if (target->zck_state == ZckState::HEADER)
            return true;
    }
    zck = zck_dl_get_zck(target->target->zck_dl);

    if (target->zck_state == ZckState::BODY_CK)
    {
        pfdebug("Checking zchunk data checksum: {}", target->target->path);
        // Check whether file has been fully downloaded
        int cks_good = zck_find_valid_chunks(zck);
        if (!cks_good)
        {
            // Error while validating checksums
            throw zchunk_error(fmt::format("Error validating zchunk file: {}", zck_get_error(zck)));
        }

        if (cks_good == 1)
        {
            // All checksums good
            pfdebug("zchunk: File is complete");
            if (target->target->zck_dl)
                zck_dl_free(&(target->target->zck_dl));
            target->zck_state = ZckState::FINISHED;
            return true;
        }

        pfdebug("Downloading rest of zchunk body: {}", target->target->path);

        // Download the remaining checksums
        zck_reset_failed_chunks(zck);
        if (!find_local_zck_chunks(target))
            return false;

        cks_good = zck_find_valid_chunks(zck);
        if (!cks_good)
        {
            // Error while validating checksums
            throw zchunk_error(fmt::format("Error validating zchunk file {}", zck_get_error(zck)));
        }

        if (cks_good == 1)
        {  // All checksums good
            if (target->target->zck_dl)
                zck_dl_free(&(target->target->zck_dl));
            target->zck_state = ZckState::FINISHED;
            return true;
        }
    }
    zck_reset_failed_chunks(zck);

    // Recalculate how many bytes remain to be downloaded by subtracting from
    // total_to_download
    target->target->downloaded = target->target->total_to_download;
    for (zckChunk* idx = zck_get_first_chunk(zck); idx != NULL; idx = zck_get_next_chunk(idx))
        if (zck_get_chunk_valid(idx) != 1)
            target->target->downloaded -= zck_get_chunk_comp_size(idx) + 92;
    return prepare_zck_body(target);
}

#endif
