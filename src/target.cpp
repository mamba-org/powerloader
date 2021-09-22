#include "target.hpp"

// CURL *Target::handle() const
// {
//     return curl_handle;
// }

void
Target::reset()
{
    // if (curl_handle)
    // {
    //     curl_easy_cleanup(curl_handle);
    //     curl_handle = nullptr;
    // }
    if (this->f != nullptr)
    {
        fclose(this->f);
        this->f = nullptr;
    }
}

bool
Target::truncate_transfer_file()
{
    std::cout << "Truncating transfer file " << std::endl;
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

std::shared_ptr<std::ofstream>
Target::open_target_file()
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
        std::cout << "RESUME: " << this->resume << " vs " << target->is_zchunk << std::endl;
        if (this->resume || target->is_zchunk)
        {
            std::cout << "Open with ate" << std::endl;
            open_flags = std::ios::app | std::ios::binary | std::ios::ate;
        }
        else
        {
            open_flags = std::ios::out | std::ios::trunc | std::ios::binary;
        }

        target->fd.reset(new std::ofstream(target->fn + PARTEXT, open_flags));
        // TODO set permissions using fs::permissions?!
        // fd = open(target->fn, open_flags, 0666);
    }

    return target->fd;
}

#ifdef WITH_ZCHUNK
/* Fail if dl_ctx->fail_no_ranges is set and we get a 200 response */
std::size_t
zckheadercb(char* buffer, std::size_t size, std::size_t nitems, Target* self)
{
    assert(self && self->target);
    std::cout << "HEADER callback ZCHUNK!" << std::endl;
    long code = -1;
    curl_easy_getinfo(self->curl_handle->ptr(), CURLINFO_RESPONSE_CODE, &code);
    if (code == 200)
    {
        pfdebug("Too many ranges were attempted in one download");
        self->range_fail = 1;
        return 0;
    }
    else
    {
        std::cout << "NOT 200 code :/" << std::endl;
    }

    return zck_header_cb(buffer, size, nitems, self->target->zck_dl);
}
#endif  // WITH_ZCHUNK

std::size_t
Target::header_callback(char* buffer, std::size_t size, std::size_t nitems, Target* self)
{
    assert(self);

    size_t ret = size * nitems;
    Target* target = self;
    HeaderCbState state = self->headercb_state;

    if (state == HeaderCbState::DONE || state == HeaderCbState::INTERRUPTED)
    {
        // Nothing to do
        return ret;
    }

#ifdef WITH_ZCHUNK
    if (target->target->is_zchunk && !target->range_fail && target->mirror
        && target->mirror->protocol == Protocol::HTTP)
        return zckheadercb(buffer, size, nitems, self);
#endif /* WITH_ZCHUNK */

    std::string_view header(buffer, size * nitems);

    if (state == HeaderCbState::DEFAULT)
    {
        if (target->protocol == Protocol::HTTP && starts_with(header, "HTTP/"))
        {
            if (contains(header, "200")
                || contains(header, "206") && !contains(header, "connection established"))
            {
                // pfdebug("Header state OK! {}", header);
                target->headercb_state = HeaderCbState::HTTP_STATE_OK;
            }
            else
            {
                // pfdebug("Header state not OK! {}", header);
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

    if (state == HeaderCbState::HTTP_STATE_OK)
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
                pfdebug("Etag: {}", value);
                // s->etag = value;
            }
            else if (lkey == "cache-control")
            {
                // s->cache_control = value;
                pfdebug("cache_control: {}", value);
            }
            else if (lkey == "last-modified")
            {
                // s->mod = value;
                pfdebug("last-modified: {}", value);
            }
            else if (lkey == "content-length")
            {
                ptrdiff_t expected = target->target->expected_size;
                ptrdiff_t content_length = std::stoll(std::string(value));
                pfdebug("Server returned Content-Length: {}", content_length);
                if (content_length > 0 && content_length != expected)
                {
                    pfdebug("Content length from server not matching {} vs {}",
                            content_length,
                            expected);
                    target->headercb_state = HeaderCbState::INTERRUPTED;
                    // target->headercb_interrupt_reason = fmt::format(
                    //     "Server reports Content-Length: {} but expected size is: {}",
                    //     content_length, expected);

                    // Return error value
                    ret++;
                }
                else
                {
                    // TODO what when we also want ETag etc.?
                    target->headercb_state = HeaderCbState::DONE;
                }
            }
        }
    }

    return ret;
}

#ifdef WITH_ZCHUNK
std::size_t
zckwritecb(char* buffer, size_t size, size_t nitems, Target* self)
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

std::size_t
Target::write_callback(char* buffer, std::size_t size, std::size_t nitems, Target* self)
{
    assert(self);

    std::size_t cur_written_expected = nitems, cur_written;

    // for (int i = 0; i < size * nitems; ++i) std::cout << buffer[i];
    // std::cout << std::endl;

#ifdef WITH_ZCHUNK
    if (self->target->is_zchunk && !self->range_fail && self->mirror
        && self->mirror->protocol == Protocol::HTTP)
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

    /* Deal with situation when user wants only specific byte range of the
     * target file, and write only the range.
     */

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

    if (cur_range_start >= range_start)
    {
        // Write the current curl passed range from the start
        ;
    }
    else
    {
        // Find the right starting offset
        assert(range_start >= cur_range_start);
        std::size_t offset = range_start - cur_range_start;
        buffer += offset;
        // Corret the length appropriately
        nitems = all - offset;
    }

    if (range_end != 0)
    {
        // End range is specified

        if (cur_range_end <= range_end)
        {
            // Write the current curl passed range to the end
            ;
        }
        else
        {
            // Find the length of the new sequence
            assert(cur_range_end >= range_end);
            std::size_t offset = cur_range_end - range_end;
            // Corret the length appropriately
            nitems -= (offset - 1);
        }
    }

    assert(nitems > 0);
    // fwrite(ptr, size, nitems, target->f);
    self->target->fd->write(buffer, size * nitems);

    // TOOD check failbit!
    // if (cur_written != nitems)
    // {
    //     g_warning("Error while writing file: %s", g_strerror(errno));
    //     return 0; // There was an error
    // }

    return cur_written_expected;

    // // std::cout << "Write callback wuiuiui ... " << std::endl;
    // // if (!this->fd) throw std::runtime_error("No ofstream open!");
    // std::size_t numbytes = size * nitems;
    // // static_cast<DownloadTarget *>(self)->fd->write(buffer, numbytes);
    // self->target->fd->write(buffer, numbytes);
    // return numbytes;
}
