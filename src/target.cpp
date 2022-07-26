#include <powerloader/target.hpp>
#ifdef WITH_ZCHUNK
#include "zck.hpp"
#endif

namespace powerloader
{
    Target::Target(const Context& ctx,
                   std::shared_ptr<DownloadTarget> dl_target,
                   std::vector<std::shared_ptr<Mirror>> mirrors)
        : m_target(dl_target)
        , m_resume(dl_target->resume())
        , m_original_offset(-1)
        , m_state(DownloadState::kWAITING)
        , m_mirrors(std::move(mirrors))
        , m_ctx(ctx)
    {
    }

    Target::~Target()
    {
        reset();
    }

    bool Target::zck_running() const
    {
#ifdef WITH_ZCHUNK
        return m_target->is_zchunck() && m_zck_state != ZckState::kFINISHED;
#else
        return false;
#endif
    }

    void Target::reset()
    {
        if (m_target->outfile() && !zck_running())
        {
            std::error_code ec;
            m_target->outfile()->close(ec);
            if (ec)
            {
                spdlog::error("Could not close file: {}", m_target->outfile()->path().string());
            }
        }

        m_headercb_interrupt_reason.clear();
    }

    void Target::reset_file(TransferStatus status)
    {
        if (m_target->outfile() && status == TransferStatus::kSUCCESSFUL)
        {
            reset();

            std::error_code ec;
            fs::rename(m_temp_file, m_target->filename(), ec);

            if (!ec && m_ctx.preserve_filetime)
            {
                auto remote_filetime = m_curl_handle->getinfo<curl_off_t>(CURLINFO_FILETIME_T);
                if (!remote_filetime || remote_filetime.value() < 0)
                    spdlog::debug("Unable to get remote time of retrieved document");

                if (remote_filetime.value() >= 0)
                {
                    fs::file_time_type tp(std::chrono::seconds(remote_filetime.value()));
                    fs::last_write_time(m_target->filename(), tp, ec);
                }
            }
        }
        else if (status == TransferStatus::kALREADYEXISTS)
        {
            reset();
        }
        else if (status == TransferStatus::kERROR)
        {
            reset();
            spdlog::error("Removing file {}", m_temp_file.string());
            fs::remove(m_temp_file);
        }
    }

    CbReturnCode Target::call_end_callback(TransferStatus status)
    {
        reset_file(status);

        CbReturnCode rc = CbReturnCode::kOK;
        if (m_target->end_callback())
        {
            if (m_curl_handle)
            {
                m_response.fill_values(*m_curl_handle);
            }
            rc = m_target->end_callback()(status, m_response);

            if (rc == CbReturnCode::kERROR)
            {
                spdlog::error("End-Callback returned an error");
            }
        }
        return rc;
    }

    bool Target::truncate_transfer_file()
    {
        std::ptrdiff_t offset = 0;
        std::error_code ec;

        if (!m_target->outfile() || !m_target->outfile()->open())
            return true;

        if (m_original_offset >= 0)
            offset = m_original_offset;

        m_target->outfile()->truncate(offset, ec);
        if (ec)
        {
            throw std::runtime_error("Could not truncate file");
        }

        m_target->outfile()->seek(offset, SEEK_SET);
        return true;
    }

    void Target::open_target_file()
    {
        // Use supplied filename
        fs::path fn = m_target->filename();
        m_temp_file = fn.replace_extension(fn.extension().string() + PARTEXT);
        spdlog::info("Opening file {}", m_temp_file.string());

        const auto open_mode = fs::exists(m_temp_file) && m_resume ? FileIO::append_update_binary
                                                                   : FileIO::write_update_binary;

        std::error_code ec;
        m_target->set_outfile(std::make_unique<FileIO>(m_temp_file, open_mode, ec));

        if (ec)
        {
            throw std::system_error(ec);
        }

        m_writecb_received = 0;
        m_writecb_required_range_written = false;
    }

#ifdef WITH_ZCHUNK
    /* Fail if dl_ctx->fail_no_ranges is set and we get a 200 response */
    std::size_t zckheadercb(char* buffer, std::size_t size, std::size_t nitems, Target* self)
    {
        assert(self && self->m_target);
        long code = -1;
        curl_easy_getinfo(self->m_curl_handle->ptr(), CURLINFO_RESPONSE_CODE, &code);
        if (code == 200)
        {
            spdlog::info("Too many ranges were attempted in one download");
            self->m_range_fail = 1;
            return 0;
        }

        return zck_header_cb(buffer, size, nitems, self->m_target->zck().zck_dl);
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
        HeaderCbState state = self->m_headercb_state;

        // TODO get rid of this?
        if (state == HeaderCbState::kDONE || state == HeaderCbState::kINTERRUPTED)
        {
            // Nothing to do
            return ret;
        }

#ifdef WITH_ZCHUNK
        if (target->target().is_zchunck() && !target->m_range_fail && target->m_mirror
            && target->m_mirror->protocol() == Protocol::kHTTP)
            return zckheadercb(buffer, size, nitems, self);
#endif /* WITH_ZCHUNK */

        std::string_view header(buffer, size * nitems);

        if (state == HeaderCbState::kDEFAULT)
        {
            if (target->m_protocol == Protocol::kHTTP && starts_with(header, "HTTP/"))
            {
                if (contains(header, "200")
                    || (contains(header, "206") && !contains(header, "connection established")))
                {
                    spdlog::info("Header state OK! {}", header);
                    target->m_headercb_state = HeaderCbState::kHTTP_STATE_OK;
                }
                else
                {
                    spdlog::info("Header state not OK! {}", header);
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
                target->m_response.headers[lkey] = value;

                if (target->target().expected_size() > 0 && lkey == "content-length")
                {
                    ptrdiff_t content_length = std::stoll(std::string(value));
                    spdlog::info("Server returned Content-Length: {}", content_length);
                    if (content_length > 0 && content_length != target->target().expected_size())
                    {
                        target->m_headercb_state = HeaderCbState::kINTERRUPTED;
                        target->m_headercb_interrupt_reason = fmt::format(
                            "Server reports Content-Length: {} but expected size is: {}",
                            content_length,
                            target->target().expected_size());

                        // Return error value
                        ret++;
                    }
                    else
                    {
                        // TODO what when we also want ETag etc.?
                        target->m_headercb_state = HeaderCbState::kDONE;
                    }
                }
            }
        }

        return ret;
    }

#ifdef WITH_ZCHUNK
    std::size_t zckwritecb(char* buffer, size_t size, size_t nitems, Target* self)
    {
        if (self->m_zck_state == ZckState::kHEADER)
        {
            spdlog::info("zck: Writing header");
            return zck_write_zck_header_cb(buffer, size, nitems, self->m_target->zck().zck_dl);
        }
        else if (self->m_zck_state == ZckState::kHEADER_LEAD)
        {
            spdlog::info("zck: Writing lead");
            return self->m_target->outfile()->write(buffer, size, nitems);
        }
        else
        {
            spdlog::info("zck: Writing body");
            return zck_write_chunk_cb(buffer, size, nitems, self->m_target->zck().zck_dl);
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
        if (self->m_target->is_zchunck() && !self->m_range_fail && self->m_mirror
            && self->m_mirror->protocol() == Protocol::kHTTP)
        {
            return zckwritecb(buffer, size, nitems, self);
        }
#endif /* WITH_ZCHUNK */

        // Total number of bytes from curl
        const std::size_t all = size * nitems;
        const std::size_t range_start = self->m_target->byterange_start();
        const std::size_t range_end = self->m_target->byterange_end();

        if (range_start <= 0 && range_end <= 0)
        {
            // Write everything curl gives us
            self->m_writecb_received += all;
            return self->m_target->outfile()->write(buffer, size, nitems);
        }

        // Deal with situation when user wants only specific byte range of the
        // target file, and write only the range.
        std::size_t cur_range_start = self->m_writecb_received;
        std::size_t cur_range_end = cur_range_start + all;

        self->m_writecb_received += all;

        if (self->m_target->byterange_start() > 0)
        {
            // If byterangestart is specified, then CURLOPT_RESUME_FROM_LARGE
            // is used by default
            cur_range_start += self->m_target->byterange_start();
            cur_range_end += self->m_target->byterange_start();
        }
        else if (self->m_original_offset > 0)
        {
            cur_range_start += self->m_original_offset;
            cur_range_end += self->m_original_offset;
        }

        if (cur_range_end < range_start)
            // The wanted byte range doesn't start yet
            return cur_written_expected;

        if (range_end != 0 && cur_range_start > range_end)
        {
            // The wanted byte range is over
            // Return zero that will lead to transfer abortion
            // with error code CURLE_WRITE_ERROR
            self->m_writecb_required_range_written = true;
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
        cur_written = self->m_target->outfile()->write(buffer, size, nitems);

        if (cur_written != nitems)
        {
            spdlog::error("Writing file {}: {}", self->m_temp_file.string(), strerror(errno));
            // There was an error
            return 0;
        }

        return cur_written;
    }

    // Progress callback for CURL handles set by the user of powerdownloader.
    int Target::progress_callback(Target* target,
                                  curl_off_t total_to_download,
                                  curl_off_t now_downloaded,
                                  curl_off_t /*total_to_upload*/,
                                  curl_off_t /*now_uploaded*/)
    {
        int ret = 0;

        assert(target);
        assert(target->m_target);

        if (target->m_state != DownloadState::kRUNNING)
        {
            return ret;
        }

        if (!target->m_target->progress_callback())
        {
            return ret;
        }

#ifdef WITH_ZCHUNK
        if (target->m_target->is_zchunck())
        {
            total_to_download = target->m_target->zck().total_to_download;
            now_downloaded = now_downloaded + target->m_target->zck().downloaded;
        }
#endif /* WITH_ZCHUNK */

        ret = target->m_target->progress_callback()(total_to_download, now_downloaded);

        // target->cb_return_code = ret;

        return ret;
    }

    bool Target::check_filesize()
    {
        if (m_target->expected_size() > 0)
        {
            if (fs::file_size(m_temp_file) != m_target->expected_size())
            {
                spdlog::error("Filesize of {} ({}) does not match expected filesize ({}).",
                              m_temp_file.string(),
                              fs::file_size(m_temp_file),
                              m_target->expected_size());
                return false;
            }
        }
        return true;
    }

    bool Target::check_checksums()
    {
        if (!m_ctx.validate_checksum)
        {
            return true;
        }

        if (m_target->checksums().empty())
        {
            return true;
        }

        return m_target->validate_checksum(m_temp_file);
    }

    void Target::change_mirror(std::shared_ptr<Mirror> mirror)
    {
        m_mirror = std::move(mirror);
    }

    CbReturnCode Target::set_failed(DownloaderError error)
    {
        m_state = DownloadState::kFAILED;
        m_target->set_error(std::move(error));

        return call_end_callback(TransferStatus::kERROR);
    }

    void Target::check_if_already_finished()
    {
        if (m_target->already_downloaded())
        {
            spdlog::info("Found already downloaded file!");
            call_end_callback(TransferStatus::kALREADYEXISTS);
            m_state = DownloadState::kFINISHED;
        }
    }

    void Target::set_to_max_speed()
    {
        assert(m_curl_handle);
        assert(m_ctx.max_speed_limit > 0);
        m_curl_handle->setopt(CURLOPT_MAX_RECV_SPEED_LARGE,
                              static_cast<curl_off_t>(m_ctx.max_speed_limit));
    }

    void Target::reset_response()
    {
        m_response = {};
    }

    void Target::prepare_for_transfer(CURLM* multi_handle,
                                      const std::string& full_url,
                                      Protocol protocol)
    {
        // Prepare CURL easy handle
        m_curl_handle.reset(new CURLHandle(m_ctx));
        CURLHandle& h = *(m_curl_handle);

        if (m_mirror && m_mirror->needs_preparation(this))
        {
            m_mirror->prepare(m_target->path(), h);
            m_state = DownloadState::kPREPARATION;

            CURLMcode cm_rc = curl_multi_add_handle(multi_handle, h);
            return;
        }

        // Set URL
        h.url(full_url);

        // Prepare FILE
#ifdef WITH_ZCHUNK
        if (!m_target->is_zchunck())
        {
#endif
            this->open_target_file();
#ifdef WITH_ZCHUNK
        }
        // If file is zchunk, prep it
        if (m_target->is_zchunck())
        {
            if (!m_target->outfile())
            {
                spdlog::info("zck: opening file {}", this->temp_file().string());
                this->open_target_file();
            }

            if (!check_zck(*this))
            {
                spdlog::error("Unable to initialize zchunk file!");
                this->set_failed(DownloaderError{
                    ErrorLevel::FATAL, ErrorCode::PD_ZCK, "Unable to initialize zchunk file" });
            }

            // If zchunk is finished, we're done, so move to next target
            if (m_zck_state == ZckState::kFINISHED)
            {
                spdlog::info("Target fully downloaded: {}", m_target->path());
                m_state = DownloadState::kFINISHED;
                reset();
                call_end_callback(
                    TransferStatus::kSUCCESSFUL);  // TODO: do something with the result?
                return;
            }
        }
#endif /* WITH_ZCHUNK */

        if (m_resume && m_resume_count >= m_ctx.max_resume_count)
        {
            m_resume = false;
            spdlog::info("Download resume ignored, maximal number of attempts has been reached");
        }

        // Resume - set offset to resume incomplete download
        if (m_resume)
        {
            m_resume_count++;
            if (m_original_offset == -1)
            {
                // Determine offset
                m_target->outfile()->seek(0L, SEEK_END);
                std::ptrdiff_t determined_offset = m_target->outfile()->tell();

                if (determined_offset == -1)
                {
                    // An error while determining offset => download the whole file again
                    determined_offset = 0;
                }
                m_original_offset = determined_offset;
            }

            curl_off_t used_offset = m_original_offset;

            spdlog::info("Trying to resume from offset {}", used_offset);
            h.setopt(CURLOPT_RESUME_FROM_LARGE, used_offset);
        }

        if (m_target->byterange_start() > 0)
        {
            assert(!m_target->resume() && m_target->range().empty());
            h.setopt(CURLOPT_RESUME_FROM_LARGE, (curl_off_t) m_target->byterange_start());
        }

        // Set range if user specified one
        if (!m_target->range().empty())
        {
            assert(!m_target->resume() && !m_target->byterange_start());
            h.setopt(CURLOPT_RANGE, m_target->range());
        }

        // Prepare progress callback
        if (m_target->progress_callback())
        {
            h.setopt(CURLOPT_XFERINFOFUNCTION, &Target::progress_callback);
            h.setopt(CURLOPT_NOPROGRESS, 0);
            h.setopt(CURLOPT_XFERINFODATA, this);
        }

        // Prepare header callback
        h.setopt(CURLOPT_HEADERFUNCTION, &Target::header_callback);
        h.setopt(CURLOPT_HEADERDATA, this);

        // Prepare write callback
        h.setopt(CURLOPT_WRITEFUNCTION, &Target::write_callback);
        h.setopt(CURLOPT_WRITEDATA, this);

        // Set extra HTTP headers
        if (this->mirror())
        {
            h.add_headers(this->mirror()->get_auth_headers(m_target->path()));
        }

        // accept default curl supported encodings
        h.accept_encoding();
        h.add_headers(m_ctx.additional_httpheaders);

        if (m_target->no_cache())
        {
            // Add headers that tell proxy to serve us fresh data
            h.add_header("Cache-Control: no-cache");
            h.add_header("Pragma: no-cache");
        }
        else
        {
            m_target->add_handle_options(h);
        }

        // Add the new handle to the curl multi handle
        CURL* handle = h;
        CURLMcode cm_rc = curl_multi_add_handle(multi_handle, handle);
        assert(cm_rc == CURLM_OK);

        // Set the state of transfer as running
        m_state = DownloadState::kRUNNING;

        // Increase running transfers counter for mirror
        if (m_mirror)
        {
            m_mirror->increase_running_transfers();
        }

        // Set the state of header callback for this transfer
        m_headercb_state = HeaderCbState::kDEFAULT;
        m_headercb_interrupt_reason.clear();

        // Set protocol of the target
        m_protocol = protocol;
    }

    tl::expected<void, DownloaderError> Target::finish_transfer(const std::string& effective_url)
    {
#ifdef WITH_ZCHUNK
        if (m_target->is_zchunck())
        {
            if (m_zck_state == ZckState::kHEADER_LEAD)
            {
                if (!zck_read_lead(*this))
                    return {};
            }
            else if (m_zck_state == ZckState::kHEADER)
            {
                if (m_mirror->stats().max_ranges > 0 && m_mirror->protocol() == Protocol::kHTTP
                    && !zck_valid_header(*this))
                {
                    return {};
                }
            }
            else if (m_zck_state == ZckState::kBODY)
            {
                if (m_mirror->stats().max_ranges > 0 && m_mirror->protocol() == Protocol::kHTTP)
                {
                    zckCtx* zck = zck_dl_get_zck(m_target->zck().zck_dl);
                    if (zck == nullptr)
                    {
                        spdlog::error("Unable to get zchunk file from download context");
                        return tl::unexpected(
                            DownloaderError{ ErrorLevel::SERIOUS,
                                             ErrorCode::PD_ZCK,
                                             "Unable to get zchunk file from download context" });
                    }
                    if (zck_failed_chunks(zck) == 0 && zck_missing_chunks(zck) == 0)
                    {
                        m_zck_state = ZckState::kFINISHED;
                    }
                }
                else
                {
                    m_zck_state = ZckState::kFINISHED;
                }
            }

            if (m_zck_state == ZckState::kFINISHED)
            {
                zckCtx* zck = zck_init_read(*this);
                if (!zck)
                    return {};
                if (zck_validate_checksums(zck) < 1)
                {
                    zck_free(&zck);  // TODO: add RAII to handle that
                    spdlog::error("At least one of the zchunk checksums doesn't match in {}",
                                  effective_url);

                    return tl::unexpected(DownloaderError{
                        ErrorLevel::SERIOUS,
                        ErrorCode::PD_BADCHECKSUM,
                        fmt::format("At least one of the zchunk checksums doesn't match in {}",
                                    effective_url) });
                }
                zck_free(&zck);  // TODO: check if it's a leak when not reached
            }
        }
        else
        {
#endif
            tl::expected<void, DownloaderError> result;

            if (m_target->outfile())
            {
                // New file was downloaded
                if (!check_filesize())
                {
                    result = tl::unexpected(
                        DownloaderError{ ErrorLevel::SERIOUS,
                                         ErrorCode::PD_BADCHECKSUM,
                                         "Result file does not have expected filesize" });
                }
                if (!check_checksums())
                {
                    result = tl::unexpected(
                        DownloaderError{ ErrorLevel::SERIOUS,
                                         ErrorCode::PD_BADCHECKSUM,
                                         "Result file does not have expected checksum" });
                }
            }

            if (!result)
            {
                reset_file(TransferStatus::kERROR);
                return result;
            }

#ifdef WITH_ZCHUNK
        }
#endif
        return {};
    }

    void Target::flush_target_file()
    {
        if (m_target->outfile() && m_target->outfile()->open())
        {
            m_target->outfile()->flush();
        }
    }

    void Target::complete_mirror_usage(bool was_success,
                                       const tl::expected<void, DownloaderError>& result)
    {
        // TODO check if we were preparing here?
        if (m_mirror)
        {
            m_tried_mirrors.insert(m_mirror);
            m_mirror->update_statistics(was_success);
            if (m_ctx.adaptive_mirror_sorting)
                sort_mirrors(
                    m_mirrors, m_mirror, was_success, result ? false : result.error().is_serious());
        }
    }

    bool Target::can_retry_transfer_with_fewer_connections() const
    {
        if (!m_mirror)
            return false;

        const auto mirror_stats = m_mirror->stats();
        return m_mirror->has_running_transfers()
               || (mirror_stats.successful_transfers > 0
                   && mirror_stats.failed_transfers < mirror_stats.max_tried_parallel_connections);
    }

    void Target::lower_mirror_parallel_connections()
    {
        if (!m_mirror)
            return;

        if (m_mirror->has_running_transfers())
        {
            const auto mirror_stats = m_mirror->stats();
            m_mirror->set_allowed_parallel_connections(mirror_stats.running_transfers);
        }
        else
        {
            m_mirror->set_allowed_parallel_connections(1);
        }

        // Give used mirror another chance
        m_tried_mirrors.erase(m_mirror);
    }

    bool Target::set_retrying()
    {
        m_state = DownloadState::kWAITING;
        m_retries++;

#ifdef WITH_ZCHUNK
        if (!m_target->is_zchunck() || m_zck_state == ZckState::kHEADER)
        {
#endif
            // Truncate file - remove downloaded garbage (error html page etc.)
            if (!truncate_transfer_file())
                return false;
#ifdef WITH_ZCHUNK
        }
#endif

        return true;
    }


    void Target::finalize_transfer(const std::string& effective_url)
    {
#ifdef WITH_ZCHUNK
        if (m_target->is_zchunck() && m_zck_state != ZckState::kFINISHED)
        {
            m_state = DownloadState::kWAITING;
            if (m_mirror)
                m_tried_mirrors.erase(m_mirror);
        }
        else
        {
#endif /* WITH_ZCHUNK */
            if (m_state == DownloadState::kRUNNING)
            {
                m_state = DownloadState::kFINISHED;
            }
            else if (m_state == DownloadState::kPREPARATION)
            {
                m_state = DownloadState::kWAITING;
            }

            // Remove xattr that states that the file is being downloaded
            // by librepo, because the file is now completely downloaded
            // and the xattr is not needed (is is useful only for resuming)
            // remove_librepo_xattr(target->target);

            // For "mirror preparation" we need to call finalize_transfer here!
            if (m_curl_handle)
                m_curl_handle->finalize_transfer();

            // only call the end callback if actually finished the download target
            if (m_state == DownloadState::kFINISHED)
            {
                const CbReturnCode rc = call_end_callback(TransferStatus::kSUCCESSFUL);
                if (rc == CbReturnCode::kERROR)
                {
                    throw fatal_download_error("Interrupted by error from end callback");
                }
            }
#ifdef WITH_ZCHUNK
        }
#endif /* WITH_ZCHUNK */
        if (m_mirror)
        {
            m_target->set_mirror_to_use(m_mirror);
        }

        m_target->set_effective_url(effective_url);
    }

}
