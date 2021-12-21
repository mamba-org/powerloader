#include <spdlog/fmt/fmt.h>

#include <zck.hpp>
#include "context.hpp"
#include "target.hpp"

namespace powerloader
{
#ifdef WITH_ZCHUNK


    zck_hash zck_hash_from_checksum(ChecksumType checksum_type)
    {
        switch (checksum_type)
        {
            case ChecksumType::kSHA1:
                return ZCK_HASH_SHA1;
            case ChecksumType::kSHA256:
                return ZCK_HASH_SHA256;
            default:
                return ZCK_HASH_UNKNOWN;
        }
    }

    zckCtx* init_zck_read(const char* checksum,
                          ChecksumType checksum_type,
                          ptrdiff_t zck_header_size,
                          int fd)
    {
        // assert(!err || *err == NULL);

        zckCtx* zck = zck_create();
        if (!zck_init_adv_read(zck, fd))
        {
            zck_free(&zck);
            throw zchunk_error("Unable to initialize zchunk file for reading");
        }
        zck_hash ct = zck_hash_from_checksum(checksum_type);
        if (ct == ZCK_HASH_UNKNOWN)
        {
            zck_free(&zck);
            throw zchunk_error(
                fmt::format("Zchunk doesn't support checksum type {}", checksum_type));
        }
        if (!zck_set_ioption(zck, ZCK_VAL_HEADER_HASH_TYPE, ct))
        {
            zck_free(&zck);
            throw zchunk_error("Error setting validation checksum type");
        }
        spdlog::info("zck: setting valid header length to {}", zck_header_size);
        if (zck_header_size > 0 && !zck_set_ioption(zck, ZCK_VAL_HEADER_LENGTH, zck_header_size))
        {
            zck_free(&zck);
            throw zchunk_error("Error setting header size");
        }
        if (!zck_set_soption(zck, ZCK_VAL_HEADER_DIGEST, checksum, strlen(checksum)))
        {
            zck_free(&zck);
            throw zchunk_error(fmt::format("Unable to set validation checksum: {}", checksum));
        }
        spdlog::info("Successful init zck read");
        return zck;
    }

    zckCtx* zck_init_read_base(const char* checksum,
                               ChecksumType checksum_type,
                               std::ptrdiff_t zck_header_size,
                               int fd)
    {
        lseek(fd, 0, SEEK_SET);
        zckCtx* zck = init_zck_read(checksum, checksum_type, zck_header_size, fd);

        if (zck == nullptr)
            return nullptr;

        if (!zck_read_lead(zck))
        {
            zck_free(&zck);
            throw zchunk_error("Unable to read zchunk lead");
        }

        if (!zck_read_header(zck))
        {
            zck_free(&zck);
            throw zchunk_error("Unable to read zchunk header");
        }
        return zck;
    }

    bool zck_valid_header_base(const char* checksum,
                               ChecksumType checksum_type,
                               std::ptrdiff_t zck_header_size,
                               int fd)
    {
        lseek(fd, 0, SEEK_SET);
        zckCtx* zck = init_zck_read(checksum, checksum_type, zck_header_size, fd);
        if (zck == nullptr)
            return false;

        if (!zck_validate_lead(zck))
        {
            zck_free(&zck);
            throw zchunk_error(fmt::format("Unable to read zchunk lead"));
        }
        zck_free(&zck);
        return true;
    }

    zckCtx* zck_init_read(DownloadTarget* target, int fd)
    {
        zckCtx* zck = nullptr;
        bool found = false;
        for (auto& chksum : target->checksums)
        {
            spdlog::info("Checking checksum: {}: {}", chksum.type, "lll");
            try
            {
                zck = zck_init_read_base(
                    chksum.checksum.data(), chksum.type, target->zck_header_size, fd);
            }
            catch (const std::exception& e)
            {
                spdlog::info("Didnt find matching header...");
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

    bool zck_valid_header(DownloadTarget* target, int fd)
    {
        for (auto& chksum : target->checksums)
        {
            if (zck_valid_header_base(
                    chksum.checksum.data(), chksum.type, target->zck_header_size, fd))
            {
                return true;
            }
        }
        throw zchunk_error(fmt::format("{}'s zchunk header doesn't match", target->path));
    }

    zckCtx* zck_init_read(Target* target)
    {
        return zck_init_read(target->target, target->target->outfile->fd());
    }

    bool zck_valid_header(Target* target)
    {
        return zck_valid_header(target->target, target->target->outfile->fd());
    }


    bool zck_clear_header(Target* target)
    {
        // assert(target && target->f && target->target && target->target->path);

        // TODO
        std::error_code ec;
        target->target->outfile->seek(0L, SEEK_END);
        target->target->outfile->truncate(0, ec);
        if (ec)
        {
            spdlog::error("Truncation went wrong!");
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

    std::vector<fs::path> get_recursive_files(fs::path dir, const std::string& suffix)
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
    int lr_copy_content(int source, int dest)
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

    bool find_local_zck_header(Target* target)
    {
        zckCtx* zck = nullptr;
        bool found = false;
        int fd = target->target->outfile->fd();

        fs::path& cache_dir = Context::instance().cache_dir;
        if (!cache_dir.empty() && fs::exists(cache_dir))
        {
            spdlog::info("Cache directory: {}", cache_dir.string());
            auto filelist = get_recursive_files(cache_dir, ".zck");

            fs::path destdir = cache_dir;
            fs::path dest = destdir / target->target->path;
            spdlog::info("Saving zck file to {}", dest.string());

            for (const auto& file : filelist)
            {
                if (dest == file)
                {
                    continue;
                }

                int chk_fd = open(file.c_str(), O_RDONLY);
                if (chk_fd < 0)
                {
                    // spdlog::info("WARNING: Unable to open {}: {}", cf, g_strerror(errno));
                    spdlog::warn("zck: Unable to open {}", file.string());
                    continue;
                }
                bool valid_header = false;
                try
                {
                    valid_header = zck_valid_header(target->target, chk_fd);
                }
                catch (zchunk_error& e)
                {
                    spdlog::info("zck: no valid header {}", e.what());
                };

                if (valid_header)
                {
                    spdlog::info("zchunk: Found file with same header at ", file.string());
                    if (lr_copy_content(chk_fd, fd) == 0
                        && ftruncate(fd, lseek(chk_fd, 0, SEEK_END)) >= 0
                        && lseek(fd, 0, SEEK_SET) == 0
                        && (zck = zck_init_read(target->target, chk_fd)))
                    {
                        found = true;
                        break;
                    }
                    else
                    {
                        spdlog::error("Error copying file");
                        // g_clear_error(&tmp_err);
                    }
                }
                close(chk_fd);
            }
        }
        else
        {
            spdlog::info("No cache directory set.");
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
            target->zck_state = ZckState::kBODY_CK;
            return true;
        }
        target->zck_state = ZckState::kHEADER;
        return true;
    }

    // TODO allow headers when not knowing header size / header checksum
    bool prepare_zck_header(Target* target)
    {
        zckCtx* zck = nullptr;
        int fd = target->target->outfile->fd();

        bool valid_header = false;
        try
        {
            valid_header = zck_valid_header(target->target, fd);
        }
        catch (const zchunk_error& e)
        {
            spdlog::info("zck: no valid header {}", e.what());
        }

        if (valid_header)
        {
            try
            {
                zck = zck_init_read(target->target, fd);
            }
            catch (const zchunk_error& e)
            {
                spdlog::info("Error reading validated header {}", e.what());
            }
            if (zck)
            {
                if (!zck_dl_set_zck(target->target->zck_dl, zck))
                {
                    throw zchunk_error("Unable to setup zchunk download context");
                }
                target->zck_state = ZckState::kBODY_CK;
                return true;
            }
        }

        lseek(fd, 0, SEEK_SET);
        zck = zck_create();
        if (!zck_init_adv_read(zck, fd))
        {
            throw zchunk_error(fmt::format("Unable to initialize zchunk file {} for reading",
                                           target->target->path));
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
        target->zck_state = ZckState::kHEADER;
        spdlog::info("Header download prepared {}", target->target->total_to_download);

        return zck_clear_header(target);
    }

    bool find_local_zck_chunks(Target* target)
    {
        assert(target && target->target && target->target->zck_dl);

        zckCtx* zck = zck_dl_get_zck(target->target->zck_dl);
        int fd = target->target->outfile->fd();
        if (zck && fd != zck_get_fd(zck) && !zck_set_fd(zck, fd))
        {
            throw zchunk_error(fmt::format("Unable to set zchunk file descriptor for {}: {}",
                                           target->target->path,
                                           zck_get_error(zck)));
        }
        // if (target->target->handle->cachedir)

        fs::path& cache_dir = Context::instance().cache_dir;
        if (!cache_dir.empty() && fs::exists(cache_dir))
        {
            spdlog::info("Cache directory: {}", cache_dir.string());
            auto filelist = get_recursive_files(cache_dir, ".zck");
            bool found = false;

            fs::path dest = cache_dir / target->target->path;

            for (const auto& file : filelist)
            {
                if (dest == file)
                {
                    continue;
                }

                int chk_fd = open(file.c_str(), O_RDONLY);
                if (chk_fd < 0)
                {
                    // spdlog::info("WARNING: Unable to open {}: {}", cf, g_strerror(errno));
                    spdlog::warn("Unable to open {}", file.string());
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
                    spdlog::warn("Error copying chunks: {}", zck_get_error(zck));
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

        // Calculate how many bytes need to be downloaded
        for (zckChunk* idx = zck_get_first_chunk(zck); idx != NULL; idx = zck_get_next_chunk(idx))
        {
            if (zck_get_chunk_valid(idx) != 1)
            {
                // Estimate of multipart overhead
                target->target->total_to_download += zck_get_chunk_comp_size(idx) + 92;
            }
        }
        target->zck_state = ZckState::kBODY;

        return true;
    }

    bool prepare_zck_body(Target* target)
    {
        zckCtx* zck = zck_dl_get_zck(target->target->zck_dl);
        int fd = target->target->outfile->fd();
        if (zck && fd != zck_get_fd(zck) && !zck_set_fd(zck, fd))
        {
            throw zchunk_error(fmt::format("Unable to set zchunk file descriptor for {}: {}",
                                           target->target->path,
                                           zck_get_error(zck)));
        }

        zck_reset_failed_chunks(zck);
        if (zck_missing_chunks(zck) == 0)
        {
            target->zck_state = ZckState::kFINISHED;
            return true;
        }

        lseek(fd, 0, SEEK_SET);

        spdlog::info("Chunks that still need to be downloaded: {}", zck_missing_chunks(zck));

        zck_dl_reset(target->target->zck_dl);
        zckRange* range
            = zck_get_missing_range(zck, target->mirror ? target->mirror->max_ranges : -1);
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
        target->zck_state = ZckState::kBODY;

        return true;
    }

    bool check_zck(Target* target)
    {
        assert(target);
        assert(target->target);
        assert(target->target->outfile->open());

        if (target->mirror
            && (target->mirror->max_ranges == 0 || target->mirror->protocol != Protocol::kHTTP))
        {
            spdlog::info("zck: mirror does not support zck");
            target->zck_state = ZckState::kBODY;
            target->target->expected_size = target->target->orig_size;
            target->target->range.clear();
            return true;
        }

        spdlog::info("checking zck");

        if (target->target->zck_dl == nullptr)
        {
            target->target->zck_dl = zck_dl_init(nullptr);
            if (target->target->zck_dl == nullptr)
            {
                throw zchunk_error(zck_get_error(nullptr));
            }
            target->zck_state = target->target->zck_header_size == -1 ? ZckState::kHEADER_LEAD
                                                                      : ZckState::kHEADER_CK;
        }

        /* Reset range fail flag */
        target->range_fail = false;

        /* If we've finished, then there's no point in checking any further */
        if (target->zck_state == ZckState::kFINISHED)
            return true;

        zckCtx* zck = zck_dl_get_zck(target->target->zck_dl);
        if (target->zck_state == ZckState::kHEADER_LEAD)
        {
            spdlog::critical("zck: download without header_size & hash not implemented yet");
            // We need to create a temporary file here, and then download the header there.
            // Then we can read the "lead" using zck_read_lead(...) from that temporary file and
            // compare with what we have on disk
            //     FILE* tf = tmpfile(void)
            //     target->target->range = zck_get_range(0, zck_get_min_download_size() - 1);
            //     target->target->total_to_download = zck_get_min_download_size();
            //     target->target->resume = false;
            //     target->zck_state = ZckState::kHEADER_LEAD;
            //     spdlog::info("Header lead download prepared {}",
            //     target->target->total_to_download);
            //     // return zck_clear_header(target);
            //     return true;
        }

        if (!zck)
        {
            target->zck_state = ZckState::kHEADER_CK;
            spdlog::debug("Unable to read zchunk header: {}", target->target->path);
            if (!find_local_zck_header(target))
                return false;
        }

        zck = zck_dl_get_zck(target->target->zck_dl);
        if (target->zck_state == ZckState::kHEADER)
        {
            if (!prepare_zck_header(target))
                return false;

            if (target->zck_state == ZckState::kHEADER)
                return true;
        }

        zck = zck_dl_get_zck(target->target->zck_dl);
        if (target->zck_state == ZckState::kBODY_CK)
        {
            spdlog::info("Checking zchunk data checksum: {}", target->target->path);
            // Check whether file has been fully downloaded
            int cks_good = zck_find_valid_chunks(zck);
            if (!cks_good)
            {
                // Error while validating checksums
                throw zchunk_error(
                    fmt::format("Error validating zchunk file: {}", zck_get_error(zck)));
            }

            if (cks_good == 1)
            {
                // All checksums good
                spdlog::info("zchunk: File is complete");
                if (target->target->zck_dl)
                    zck_dl_free(&(target->target->zck_dl));
                target->zck_state = ZckState::kFINISHED;
                return true;
            }

            spdlog::debug("Downloading rest of zchunk body: {}", target->target->path);

            // Download the remaining checksums
            zck_reset_failed_chunks(zck);
            if (!find_local_zck_chunks(target))
                return false;

            cks_good = zck_find_valid_chunks(zck);
            if (!cks_good)
            {
                // Error while validating checksums
                throw zchunk_error(
                    fmt::format("Error validating zchunk file {}", zck_get_error(zck)));
            }

            if (cks_good == 1)
            {  // All checksums good
                if (target->target->zck_dl)
                    zck_dl_free(&(target->target->zck_dl));
                target->zck_state = ZckState::kFINISHED;
                return true;
            }
        }
        zck_reset_failed_chunks(zck);

        // Recalculate how many bytes remain to be downloaded by subtracting from
        // total_to_download
        target->target->downloaded = target->target->total_to_download;
        for (zckChunk* idx = zck_get_first_chunk(zck); idx != nullptr;
             idx = zck_get_next_chunk(idx))
            if (zck_get_chunk_valid(idx) != 1)
                target->target->downloaded -= zck_get_chunk_comp_size(idx) + 92;
        return prepare_zck_body(target);
    }

#endif
}
