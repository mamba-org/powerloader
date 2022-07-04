#include <spdlog/fmt/fmt.h>

#include "zck.hpp"
#include <powerloader/context.hpp>
#include <powerloader/target.hpp>
#include <powerloader/download_target.hpp>

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
    ChecksumType checksum_type_from_zck_hash(zck_hash hash_type)
    {
        switch (hash_type)
        {
            case ZCK_HASH_SHA1:
                return ChecksumType::kSHA1;
            case ZCK_HASH_SHA256:
                return ChecksumType::kSHA256;
            default:
                throw zchunk_error("zck: Hash type not known");
        }
    }

    zckCtx* init_zck_read(const std::unique_ptr<Checksum>& checksum,
                          ptrdiff_t zck_header_size,
                          int fd)
    {
        // assert(!err || *err == NULL);
        if (!checksum)
        {
            throw zchunk_error("No header checksum set");
        }
        zckCtx* zck = zck_create();
        if (!zck_init_adv_read(zck, fd))
        {
            zck_free(&zck);
            throw zchunk_error("Unable to initialize zchunk file for reading");
        }
        zck_hash ct = zck_hash_from_checksum(checksum->type);
        if (ct == ZCK_HASH_UNKNOWN)
        {
            zck_free(&zck);
            throw zchunk_error(fmt::format("Zchunk doesn't support checksum type {}", ct));
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
        if (!zck_set_soption(
                zck, ZCK_VAL_HEADER_DIGEST, checksum->checksum.data(), checksum->checksum.size()))
        {
            zck_free(&zck);
            throw zchunk_error(
                fmt::format("Unable to set validation checksum: {}", checksum->checksum));
        }
        spdlog::info("Successful init zck read");
        return zck;
    }

    zckCtx* zck_init_read_base(const std::unique_ptr<Checksum>& checksum,
                               std::ptrdiff_t zck_header_size,
                               int fd)
    {
        lseek(fd, 0, SEEK_SET);
        zckCtx* zck = init_zck_read(checksum, zck_header_size, fd);

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

    bool zck_valid_header_base(const std::unique_ptr<Checksum>& checksum,
                               std::ptrdiff_t zck_header_size,
                               int fd)
    {
        lseek(fd, 0, SEEK_SET);
        zckCtx* zck = init_zck_read(checksum, zck_header_size, fd);
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

    zckCtx* zck_init_read(const std::shared_ptr<DownloadTarget>& target, int fd)
    {
        zckCtx* zck = nullptr;
        bool found = false;
        zck = zck_init_read_base(
            target->p_zck->zck_header_checksum, target->p_zck->zck_header_size, fd);
        return zck;
    }

    bool zck_valid_header(const std::shared_ptr<DownloadTarget>& target, int fd)
    {
        if (zck_valid_header_base(
                target->p_zck->zck_header_checksum, target->p_zck->zck_header_size, fd))
        {
            return true;
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

    bool find_local_zck_header(Target* target)
    {
        zckCtx* zck = nullptr;
        bool found = false;
        int fd = target->target->outfile->fd();
        auto dt = target->target;

        if (!dt->p_zck->zck_cache_file.empty() && fs::exists(dt->p_zck->zck_cache_file))
        {
            std::error_code ec;
            FileIO chk_file(dt->p_zck->zck_cache_file, FileIO::read_binary, ec);
            if (ec)
            {
                spdlog::warn("zck: Unable to open {} ({})", chk_file.path().string(), ec.message());
                return false;
            }
            bool valid_header = false;
            try
            {
                valid_header = zck_valid_header(target->target, chk_file.fd());
            }
            catch (zchunk_error& e)
            {
                spdlog::info("zck: no valid header {}", e.what());
            };

            if (valid_header)
            {
                spdlog::info("zchunk: Found file with same header at {}", chk_file.path().string());
                bool result = target->target->outfile->replace_from(chk_file);
                if ((zck = zck_init_read(target)))
                {
                    found = true;
                }
                else
                {
                    spdlog::error("Error copying file");
                }
            }
        }
        else
        {
            spdlog::info("No zchunk cache file set or found");
        }

        if (found)
        {
            zckCtx* old_zck = zck_dl_get_zck(target->target->p_zck->zck_dl);
            zck_free(&old_zck);
            if (!zck_dl_set_zck(target->target->p_zck->zck_dl, zck))
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
                if (!zck_dl_set_zck(target->target->p_zck->zck_dl, zck))
                {
                    throw zchunk_error("Unable to setup zchunk download context");
                }
                target->zck_state = ZckState::kBODY_CK;
                return true;
            }
        }

        target->target->outfile->seek(0, SEEK_SET);
        zck = zck_create();
        if (!zck_init_adv_read(zck, fd))
        {
            throw zchunk_error(fmt::format("Unable to initialize zchunk file {} for reading",
                                           target->target->path));
        }

        if (target->target->p_zck->zck_dl)
        {
            zckCtx* old_zck = zck_dl_get_zck(target->target->p_zck->zck_dl);
            zck_free(&old_zck);
            if (!zck_dl_set_zck(target->target->p_zck->zck_dl, zck))
            {
                throw zchunk_error(fmt::format("Unable to setup zchunk download context for {}",
                                               target->target->path));
            }
        }
        else
        {
            target->target->p_zck->zck_dl = zck_dl_init(zck);
        }

        target->target->range = zck_get_range(0, target->target->p_zck->zck_header_size - 1);
        target->target->p_zck->total_to_download = target->target->p_zck->zck_header_size;
        target->target->resume = false;
        target->zck_state = ZckState::kHEADER;
        spdlog::info("Header download prepared {}", target->target->p_zck->total_to_download);

        // Note: this truncates the header
        std::error_code ec;
        target->target->outfile->truncate(0, ec);
        if (ec)
        {
            spdlog::error("Could not truncate zchunk file");
            return false;
        }
        target->target->outfile->seek(0, SEEK_SET);
        return true;
    }

    bool find_local_zck_chunks(Target* target)
    {
        assert(target && target->target && target->target->p_zck->zck_dl);

        zckCtx* zck = zck_dl_get_zck(target->target->p_zck->zck_dl);
        int fd = target->target->outfile->fd();
        if (zck && fd != zck_get_fd(zck) && !zck_set_fd(zck, fd))
        {
            throw zchunk_error(fmt::format("Unable to set zchunk file descriptor for {}: {}",
                                           target->target->path,
                                           zck_get_error(zck)));
        }

        auto dt = target->target;
        if (!dt->p_zck->zck_cache_file.empty() && fs::exists(dt->p_zck->zck_cache_file))
        {
            std::error_code ec;
            FileIO chk_file(dt->p_zck->zck_cache_file, FileIO::read_binary, ec);

            if (ec)
            {
                spdlog::warn("Unable to open {}: {}", chk_file.path().string(), ec.message());
                return false;
            }

            zckCtx* zck_src = zck_create();
            if (!zck_init_read(zck_src, chk_file.fd()))
            {
                return false;
            }

            if (!zck_copy_chunks(zck_src, zck))
            {
                spdlog::warn("Error copying chunks from {} to {}: {}",
                             chk_file.path().string(),
                             dt->outfile->path().string(),
                             zck_get_error(zck));
                zck_free(&zck_src);
                return false;
            }
            zck_free(&zck_src);
        }

        target->target->p_zck->downloaded = target->target->p_zck->total_to_download;

        // Calculate how many bytes need to be downloaded
        for (zckChunk* idx = zck_get_first_chunk(zck); idx != NULL; idx = zck_get_next_chunk(idx))
        {
            if (zck_get_chunk_valid(idx) != 1)
            {
                // Estimate of multipart overhead
                target->target->p_zck->total_to_download += zck_get_chunk_comp_size(idx) + 92;
            }
        }
        target->zck_state = ZckState::kBODY;

        return true;
    }

    bool prepare_zck_body(Target* target)
    {
        zckCtx* zck = zck_dl_get_zck(target->target->p_zck->zck_dl);
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

        zck_dl_reset(target->target->p_zck->zck_dl);
        zckRange* range
            = zck_get_missing_range(zck, target->mirror ? target->mirror->max_ranges : -1);
        zckRange* old_range = zck_dl_get_range(target->target->p_zck->zck_dl);
        if (old_range)
        {
            zck_range_free(&old_range);
        }

        if (!zck_dl_set_range(target->target->p_zck->zck_dl, range))
        {
            throw zchunk_error("Unable to set range for zchunk downloader");
            return false;
        }

        target->target->range = zck_get_range_char(zck, range);
        target->target->expected_size = 1;
        target->zck_state = ZckState::kBODY;

        return true;
    }

    bool zck_read_lead(Target* target)
    {
        zckCtx* zck = zck_create();
        target->target->outfile->flush();
        target->target->outfile->seek(0, SEEK_SET);

        if (!zck_init_adv_read(zck, target->target->outfile->fd()))
        {
            zck_free(&zck);
            spdlog::error("Unable to initialize zchunk file for reading");
            return false;
        }

        bool success = zck_read_lead(zck);
        if (!success)
        {
            spdlog::error("Could not read lead");
            return false;
        }

        ssize_t header_length = zck_get_header_length(zck);
        char* digest = zck_get_header_digest(zck);
        ChecksumType cktype = checksum_type_from_zck_hash((zck_hash) zck_get_full_hash_type(zck));

        spdlog::info("zck: Found header size: {}\n", header_length);
        spdlog::info("zck: Found header digest: {} ({})\n", digest, strlen(digest));

        if (!target->target->p_zck->zck_header_checksum)
            target->target->p_zck->zck_header_checksum
                = std::make_unique<Checksum>(Checksum{ ChecksumType::kSHA256, digest });
        if (target->target->p_zck->zck_header_size == -1)
            target->target->p_zck->zck_header_size = header_length;
        target->zck_state = ZckState::kHEADER_CK;

        free(digest);
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
            spdlog::info("zck: mirror does not support ranges");
            target->zck_state = ZckState::kBODY;
            target->target->expected_size = target->target->orig_size;
            target->target->range.clear();
            return true;
        }

        spdlog::info("checking zck");

        if (target->target->p_zck->zck_dl == nullptr)
        {
            target->target->p_zck->zck_dl = zck_dl_init(nullptr);
            if (target->target->p_zck->zck_dl == nullptr)
            {
                throw zchunk_error(zck_get_error(nullptr));
            }
            target->zck_state = target->target->p_zck->zck_header_size == -1
                                    ? ZckState::kHEADER_LEAD
                                    : ZckState::kHEADER_CK;
        }

        /* Reset range fail flag */
        target->range_fail = false;

        /* If we've finished, then there's no point in checking any further */
        if (target->zck_state == ZckState::kFINISHED)
            return true;

        zckCtx* zck = zck_dl_get_zck(target->target->p_zck->zck_dl);
        if (target->zck_state == ZckState::kHEADER_LEAD)
        {
            spdlog::info("zck: downloading lead to find header size and hash");

            // We need to create a temporary file here, and then download the header there.
            // Then we can read the "lead" using zck_read_lead(...) from that temporary file and
            // compare with what we have on disk
            target->target->range = zck_get_range(0, zck_get_min_download_size() - 1);
            target->target->p_zck->total_to_download = zck_get_min_download_size();
            target->target->resume = false;
            target->zck_state = ZckState::kHEADER_LEAD;
            spdlog::info("Header lead download prepared {}",
                         target->target->p_zck->total_to_download);
            return true;
        }

        if (!zck)
        {
            target->zck_state = ZckState::kHEADER_CK;
            if (!find_local_zck_header(target))
                return false;
        }

        zck = zck_dl_get_zck(target->target->p_zck->zck_dl);
        if (target->zck_state == ZckState::kHEADER)
        {
            if (!prepare_zck_header(target))
                return false;

            if (target->zck_state == ZckState::kHEADER)
            {
                target->target->outfile->seek(0, SEEK_SET);
                return true;
            }
        }

        zck = zck_dl_get_zck(target->target->p_zck->zck_dl);
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
                if (target->target->p_zck->zck_dl)
                    zck_dl_free(&(target->target->p_zck->zck_dl));
                target->zck_state = ZckState::kFINISHED;
                return true;
            }

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
                if (target->target->p_zck->zck_dl)
                    zck_dl_free(&(target->target->p_zck->zck_dl));
                target->zck_state = ZckState::kFINISHED;
                return true;
            }
        }
        zck_reset_failed_chunks(zck);

        // Recalculate how many bytes remain to be downloaded by subtracting from
        // total_to_download
        target->target->p_zck->downloaded = target->target->p_zck->total_to_download;
        for (zckChunk* idx = zck_get_first_chunk(zck); idx != nullptr;
             idx = zck_get_next_chunk(idx))
            if (zck_get_chunk_valid(idx) != 1)
                target->target->p_zck->downloaded -= zck_get_chunk_comp_size(idx) + 92;
        return prepare_zck_body(target);
    }

    bool zck_extract(const fs::path& source, const fs::path& dst, bool validate)
    {
        spdlog::info("zck: extracting from {} to {}", source.string(), dst.string());
        zckCtx* zck = zck_create();
        std::error_code ec;
        FileIO sf(source, FileIO::read_binary, ec), of(dst, FileIO::write_update_binary, ec);

        if (!zck_init_read(zck, sf.fd()))
        {
            spdlog::error("{}", zck_get_error(zck));
            // goto error2;
        }

        if (validate)
        {
            spdlog::critical("Not implemented yet");
        }
        constexpr std::size_t BUF_SIZE = 32'768;

        std::vector<char> buf(BUF_SIZE);

        size_t total = 0;
        while (true)
        {
            size_t read = zck_read(zck, buf.data(), BUF_SIZE);
            if (read < 0)
            {
                spdlog::error("Error reading file {}: {}", source.string(), zck_get_error(zck));
                // goto error2;
            }
            if (read == 0)
            {
                break;
            }
            if (of.write(buf.data(), 1, read) != read)
            {
                spdlog::error("Error writing to {}", dst.string());
                // goto error2;
            }
            total += read;
        }
        if (!zck_close(zck))
        {
            spdlog::error("zck: Error closing {}", zck_get_error(zck));
            // goto error2;
        }
        // if(arguments.log_level <= ZCK_LOG_INFO)
        //     dprintf(STDERR_FILENO, "Decompressed %lu bytes\n", (unsigned long)total);
        // }
        zck_free(&zck);
        return true;
    }
#endif
}
