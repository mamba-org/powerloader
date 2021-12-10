#include "target.hpp"

namespace powerloader
{
    void Target::reset()
    {
        // curl_handle.reset();
        if (this->f != nullptr)
        {
            fclose(this->f);
            this->f = nullptr;
        }
    }

    CbReturnCode Target::call_endcallback(TransferStatus status)
    {
        EndCb end_cb = override_endcb ? override_endcb : target->endcb;
        void* cb_data = override_endcb ? override_endcb_data : target->cbdata;
        CbReturnCode rc = CbReturnCode::kOK;
        if (end_cb)
        {
            // TODO fill in message?!
            std::string message = "";
            rc = end_cb(status, message, cb_data);

            if (rc == CbReturnCode::kERROR)
            {
                cb_return_code = CbReturnCode::kERROR;
                spdlog::error("End-Callback returned an error");
            }
        }

        if (status == TransferStatus::kSUCCESSFUL)
        {
            reset();
            fs::rename(temp_file, target->fn);
        }
        else if (status == TransferStatus::kALREADYEXISTS)
        {
            // TODO not emitted currently
        }
        else if (status == TransferStatus::kERROR)
        {
            // TODO remove or can we continue?
            // fs::remove(temp_file);
        }

        return rc;
    }

    bool Target::truncate_transfer_file()
    {
        spdlog::info("Truncating transfer file ");
        std::ptrdiff_t offset = 0;

        auto p = fs::path(target->fn);

        if (!fs::exists(p))
            return true;

        if (original_offset >= 0)
            offset = original_offset;

        fs::resize_file(p, offset);
        if (target->fd && target->fd->is_open())
        {
            target->fd->seekp(original_offset);
        }
        return true;
    }

    std::shared_ptr<std::ofstream> Target::open_target_file()
    {
        /** Open the file to write to */
        if (target->fd && target->fd->is_open())
        {
            // Use supplied filedescriptor
            return target->fd;
            // if (fd == -1)
            // {
            //     g_set_error(err, LR_DOWNLOADER_ERROR, LRE_IO,
            //                 "dup(%d) failed: %s",
            //                 target->target->fd, g_strerror(errno));
            //     return nullptr;
            // }
        }
        else
        {
            // Use supplied filename
            // int open_flags = O_CREAT | O_TRUNC | O_RDWR;
            std::ios::openmode open_flags;
            if (this->resume || target->is_zchunk)
            {
                open_flags = std::ios::app | std::ios::binary | std::ios::ate;
            }
            else
            {
                open_flags = std::ios::out | std::ios::trunc | std::ios::binary;
            }

            temp_file = target->fn + PARTEXT;
            target->fd.reset(new std::ofstream(temp_file, open_flags));
            // TODO set permissions using fs::permissions?!
            // fd = open(target->fn, open_flags, 0666);
        }

        return target->fd;
    }

#ifdef WITH_ZCHUNK
    /* Fail if dl_ctx->fail_no_ranges is set and we get a 200 response */
    std::size_t zckheadercb(char* buffer, std::size_t size, std::size_t nitems, Target* self)
    {
        assert(self && self->target);
        long code = -1;
        curl_easy_getinfo(self->curl_handle->ptr(), CURLINFO_RESPONSE_CODE, &code);
        if (code == 200)
        {
            spdlog::info("Too many ranges were attempted in one download");
            self->range_fail = 1;
            return 0;
        }
        else
        {
            // TODO?
        }

        return zck_header_cb(buffer, size, nitems, self->target->zck_dl);
    }
#endif  // WITH_ZCHUNK

    std::size_t Target::header_callback(char* buffer,
                                        std::size_t size,
                                        std::size_t nitems,
                                        Target* self)
    {
        assert(self);

        size_t ret = size * nitems;
        Target* target = self;
        HeaderCbState state = self->headercb_state;

        if (state == HeaderCbState::kDONE || state == HeaderCbState::kINTERRUPTED)
        {
            // Nothing to do
            return ret;
        }

#ifdef WITH_ZCHUNK
        if (target->target->is_zchunk && !target->range_fail && target->mirror
            && target->mirror->protocol == Protocol::kHTTP)
            return zckheadercb(buffer, size, nitems, self);
#endif /* WITH_ZCHUNK */

        std::string_view header(buffer, size * nitems);

        if (state == HeaderCbState::kDEFAULT)
        {
            if (target->protocol == Protocol::kHTTP && starts_with(header, "HTTP/"))
            {
                if (contains(header, "200")
                    || contains(header, "206") && !contains(header, "connection established"))
                {
                    // spdlog::info("Header state OK! {}", header);
                    target->headercb_state = HeaderCbState::kHTTP_STATE_OK;
                }
                else
                {
                    // spdlog::info("Header state not OK! {}", header);
                }
            }
            // else if (lrtarget->protocol == LR_PROTOCOL_FTP)
            // {
            //     // Headers of a FTP protocol
            //     if (g_str_has_prefix(header, "213 "))
            //     {
            //         // Code 213 should keep the file size
            //         gint64 content_length = g_ascii_strtoll(header + 4, NULL, 0);

            //         g_debug("%s: Server returned size: \"%s\" "
            //                 "(converted %" G_GINT64_FORMAT "/%" G_GINT64_FORMAT
            //                 " expected)",
            //                 __func__, header + 4, content_length, expected);

            //         // Compare expected size and size reported by a FTP server
            //         if (content_length > 0 && content_length != expected)
            //         {
            //             g_debug("%s: Size doesn't match (%" G_GINT64_FORMAT
            //                     " != %" G_GINT64_FORMAT ")",
            //                     __func__, content_length, expected);
            //             lrtarget->headercb_state = LR_HCS_INTERRUPTED;
            //             lrtarget->headercb_interrupt_reason = g_strdup_printf(
            //                 "FTP server reports size: %" G_GINT64_FORMAT " "
            //                 "via 213 code, but expected size is: %" G_GINT64_FORMAT,
            //                 content_length, expected);
            //             ret++; // Return error value
            //         }
            //         else
            //         {
            //             lrtarget->headercb_state = LR_HCS_DONE;
            //         }
            //     }
            //     else if (g_str_has_prefix(header, "150"))
            //     {
            //         // Code 150 should keep the file size
            //         // TODO: See parse150 in /usr/lib64/python2.7/ftplib.py
            //     }
            // }
        }

        if (state == HeaderCbState::kHTTP_STATE_OK)
        {
            auto colon_idx = header.find(':');
            if (colon_idx != std::string_view::npos)
            {
                std::string_view key, value;
                key = header.substr(0, colon_idx);
                colon_idx++;
                // remove spaces
                while (std::isspace(header[colon_idx]))
                {
                    ++colon_idx;
                }

                // remove \r\n header ending
                value = header.substr(colon_idx, header.size() - colon_idx - 2);
                // http headers are case insensitive!
                std::string lkey = to_lower(key);
                if (lkey == "etag")
                {
                    spdlog::info("Etag: {}", value);
                    // s->etag = value;
                }
                else if (lkey == "cache-control")
                {
                    // s->cache_control = value;
                    spdlog::info("cache_control: {}", value);
                }
                else if (lkey == "last-modified")
                {
                    // s->mod = value;
                    spdlog::info("last-modified: {}", value);
                }
                else if (lkey == "content-length")
                {
                    ptrdiff_t expected = target->target->expected_size;
                    ptrdiff_t content_length = std::stoll(std::string(value));
                    spdlog::info("Server returned Content-Length: {}", content_length);
                    if (content_length > 0 && content_length != expected)
                    {
                        target->headercb_state = HeaderCbState::kINTERRUPTED;
                        target->headercb_interrupt_reason = fmt::format(
                            "Server reports Content-Length: {} but expected size is: {}",
                            content_length,
                            expected);

                        // Return error value
                        ret++;
                    }
                    else
                    {
                        // TODO what when we also want ETag etc.?
                        target->headercb_state = HeaderCbState::kDONE;
                    }
                }
            }
        }

        return ret;
    }

#ifdef WITH_ZCHUNK
    std::size_t zckwritecb(char* buffer, size_t size, size_t nitems, Target* self)
    {
        if (self->zck_state == ZckState::HEADER)
        {
            return zck_write_zck_header_cb(buffer, size, nitems, self->target->zck_dl);
        }
        else
        {
            return zck_write_chunk_cb(buffer, size, nitems, self->target->zck_dl);
        }
    }
#endif

    std::size_t Target::write_callback(char* buffer,
                                       std::size_t size,
                                       std::size_t nitems,
                                       Target* self)
    {
        assert(self);

        std::size_t cur_written_expected = nitems, cur_written;

#ifdef WITH_ZCHUNK
        if (self->target->is_zchunk && !self->range_fail && self->mirror
            && self->mirror->protocol == Protocol::kHTTP)
            return zckwritecb(buffer, size, nitems, self);
#endif /* WITH_ZCHUNK */

        // Total number of bytes from curl
        std::size_t all = size * nitems;
        std::size_t range_start = self->target->byterange_start;
        std::size_t range_end = self->target->byterange_end;

        if (range_start <= 0 && range_end <= 0)
        {
            // Write everything curl give to you
            self->writecb_received += all;
            // TODO check bad bit here!
            self->target->fd->write(buffer, all);
            return all;
        }

        // Deal with situation when user wants only specific byte range of the
        // target file, and write only the range.
        std::size_t cur_range_start = self->writecb_received;
        std::size_t cur_range_end = cur_range_start + all;

        self->writecb_received += all;

        if (self->target->byterange_start > 0)
        {
            // If byterangestart is specified, then CURLOPT_RESUME_FROM_LARGE
            // is used by default
            cur_range_start += self->target->byterange_start;
            cur_range_end += self->target->byterange_start;
        }
        else if (self->original_offset > 0)
        {
            cur_range_start += self->original_offset;
            cur_range_end += self->original_offset;
        }

        if (cur_range_end < range_start)
            // The wanted byte range doesn't start yet
            return cur_written_expected;

        if (range_end != 0 && cur_range_start > range_end)
        {
            // The wanted byte range is over
            // Return zero that will lead to transfer abortion
            // with error code CURLE_WRITE_ERROR
            self->writecb_required_range_written = true;
            return 0;
        }

        size = 1;
        nitems = all;

        if (cur_range_start < range_start)
        {
            // Find the right starting offset
            assert(range_start > cur_range_start);
            std::size_t offset = range_start - cur_range_start;
            buffer += offset;
            // Correct the length appropriately
            nitems = all - offset;
        }

        if (range_end != 0)
        {
            // End range is specified
            if (cur_range_end > range_end)
            {
                // Find the length of the new sequence
                std::size_t offset = cur_range_end - range_end;
                // Correct the length appropriately
                nitems -= (offset - 1);
            }
        }

        assert(nitems > 0);
        self->target->fd->write(buffer, size * nitems);

        // TOOD check failbit!
        // if (cur_written != nitems)
        // {
        //     g_warning("Error while writing file: %s", g_strerror(errno));
        //     return 0; // There was an error
        // }

        return cur_written_expected;

        // // if (!this->fd) throw std::runtime_error("No ofstream open!");
        // std::size_t numbytes = size * nitems;
        // // static_cast<DownloadTarget *>(self)->fd->write(buffer, numbytes);
        // self->target->fd->write(buffer, numbytes);
        // return numbytes;
    }

    // Progress callback for CURL handles.
    // progress callback set by the user of powerdownloader.
    int Target::progress_callback(Target* target,
                                  curl_off_t total_to_download,
                                  curl_off_t now_downloaded,
                                  curl_off_t total_to_upload,
                                  curl_off_t now_uploaded)
    {
        int ret = 0;

        assert(target);
        assert(target->target);

        if (target->state != DownloadState::kRUNNING)
        {
            return ret;
        }

        if (!target->target->progress_callback)
        {
            return ret;
        }

#ifdef WITH_ZCHUNK
        // if (target->target->is_zchunk)
        // {
        //     total_to_download = target->target->total_to_download;
        //     now_downloaded = now_downloaded + target->target->downloaded;
        // }
#endif /* WITH_ZCHUNK */

        ret = target->target->progress_callback(total_to_download, now_downloaded);

        // target->cb_return_code = ret;

        return ret;
    }

    bool Target::check_filesize()
    {
        if (target->expected_size > 0)
        {
            return fs::file_size(temp_file) == target->expected_size;
        }
        return true;
    }

    bool Target::check_checksums()
    {
        if (target->checksums.empty())
        {
            return true;
        }

        auto findchecksum = [&](const ChecksumType& t) -> Checksum* {
            for (auto& cs : target->checksums)
            {
                if (cs.type == t)
                    return &cs;
            }
            return nullptr;
        };
        Checksum* cs;
        if ((cs = findchecksum(ChecksumType::kSHA256)))
        {
            spdlog::info("Checking SHA256 sum");
            auto sum = sha256sum(temp_file);
            if (sum != cs->checksum)
            {
                spdlog::error("SHA256 sum of downloaded file is wrong.\nIs {}. Should be {}",
                              sum,
                              cs->checksum);
                fs::remove(temp_file);
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
            auto sum = md5sum(temp_file);
            if (sum != cs->checksum)
            {
                spdlog::error(
                    "MD5 sum of downloaded file is wrong.\nIs {}. Should be {}", sum, cs->checksum);
                fs::remove(temp_file);
                return false;
            }
            return true;
        }
        return true;
    }
}
