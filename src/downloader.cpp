#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>

#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <spdlog/fmt/fmt.h>

namespace fs = std::filesystem;

#include <powerloader/context.hpp>
#include <powerloader/curl.hpp>
#include <powerloader/download_target.hpp>
#include <powerloader/downloader.hpp>
#include <powerloader/enums.hpp>
#include <powerloader/mirror.hpp>
#include <powerloader/target.hpp>
#include <powerloader/utils.hpp>
#ifdef WITH_ZCHUNK
#include "zck.hpp"
#endif

namespace powerloader
{
    Downloader::Downloader(const Context& ctx)
        : ctx(ctx)
    {
        max_parallel_connections = ctx.max_parallel_downloads;
        multi_handle = curl_multi_init();
        curl_multi_setopt(multi_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, max_parallel_connections);
    }

    Downloader::~Downloader()
    {
        curl_multi_cleanup(multi_handle);
    }

    void Downloader::add(const std::shared_ptr<DownloadTarget>& dl_target)
    {
        // this function converts a DownloadTarget into a "Target"
        if (!dl_target)
            return;

        if (ctx.mirror_map.find(dl_target->base_url) != ctx.mirror_map.end())
        {
            m_targets.emplace_back(
                new Target(ctx, dl_target, ctx.mirror_map.at(dl_target->base_url)));
            dl_target->base_url.clear();
        }
        else
        {
            m_targets.emplace_back(new Target(ctx, dl_target));
        }
    }

    /** Check the finished transfer
     * Evaluate CURL return code and status code of protocol if needed.
     * @param serious_error     Serious error is an error that isn't fatal,
     *                          but mirror that generate it should be penalized.
     *                          E.g.: Connection timeout - a mirror we are unable
     *                          to connect at is pretty useless for us, but
     *                          this could be only temporary state.
     *                          No fatal but also no good.
     * @param fatal_error       An error that cannot be recovered - e.g.
     *                          we cannot write to a socket, we cannot write
     *                          data to disk, bad function argument, ...
     */
    tl::expected<void, DownloaderError> Downloader::check_finished_transfer_status(CURLMsg* msg,
                                                                                   Target* target)
    {
        long code = 0;
        char* effective_url = NULL;

        assert(msg);
        assert(target);

        curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &effective_url);

        if (msg->data.result != CURLE_OK)
        {
            // There was an error that is reported by CURLcode
            if (msg->data.result == CURLE_WRITE_ERROR && target->writecb_required_range_written)
            {
                // Download was interrupted by writecb because
                // user want only specified byte range of the
                // target and the range was already downloaded
                spdlog::info("Transfer was interrupted by writecb() because the required "
                             "range ({} - {}) was downloaded.",
                             target->target->byterange_start,
                             target->target->byterange_end);
            }
            else if (target->headercb_state == HeaderCbState::kINTERRUPTED)
            {
                // Download was interrupted by header callback
                return tl::unexpected(
                    DownloaderError{ ErrorLevel::FATAL,
                                     ErrorCode::PD_CBINTERRUPTED,
                                     fmt::format("Interrupted by header callback: {}",
                                                 target->headercb_interrupt_reason) });
            }
#ifdef WITH_ZCHUNK
            else if (target->range_fail)
            {
                zckRange* range = zck_dl_get_range(target->target->p_zck->zck_dl);
                int range_count = zck_get_range_count(range);
                if (target->mirror->max_ranges >= range_count)
                {
                    target->mirror->max_ranges = range_count / 2;
                    spdlog::debug("Setting mirror max_ranges to {}", target->mirror->max_ranges);
                }
            }
            else if (target->target->p_zck->zck_dl != nullptr
                     && zck_is_error(zck_dl_get_zck(target->target->p_zck->zck_dl)) > 0)
            {
                zckCtx* zck = zck_dl_get_zck(target->target->p_zck->zck_dl);

                // Something went wrong while writing the zchunk file
                if (zck_is_error(zck) == 1)
                {
                    // Non-fatal zchunk error
                    spdlog::warn("Serious zchunk error: {}", zck_get_error(zck));
                    return tl::unexpected(DownloaderError{
                        ErrorLevel::SERIOUS, ErrorCode::PD_ZCK, fmt::format(zck_get_error(zck)) });
                }
                else
                {
                    // Fatal zchunk error (zck_is_error(zck) == 2)
                    spdlog::error("Fatal zchunk error: {}", zck_get_error(zck));
                    return tl::unexpected(DownloaderError{
                        ErrorLevel::FATAL, ErrorCode::PD_ZCK, fmt::format(zck_get_error(zck)) });
                }
            }
#endif /* WITH_ZCHUNK */
            else
            {
                // There was a CURL error
                std::string error = fmt::format("CURL error ({}): {} for {} [{}]",
                                                msg->data.result,
                                                curl_easy_strerror(msg->data.result),
                                                effective_url,
                                                target->errorbuffer);
                spdlog::error(error);
                switch (msg->data.result)
                {
                    case CURLE_ABORTED_BY_CALLBACK:
                    case CURLE_BAD_FUNCTION_ARGUMENT:
                    case CURLE_CONV_REQD:
                    case CURLE_COULDNT_RESOLVE_PROXY:
                    case CURLE_FILESIZE_EXCEEDED:
                    case CURLE_INTERFACE_FAILED:
                    case CURLE_NOT_BUILT_IN:
                    case CURLE_OUT_OF_MEMORY:
                    // case CURLE_RECV_ERROR:  // See RhBug: 1219817
                    // case CURLE_SEND_ERROR:
                    case CURLE_SSL_CACERT_BADFILE:
                    case CURLE_SSL_CRL_BADFILE:
                    case CURLE_WRITE_ERROR:
                        // Fatal error
                        return tl::unexpected(
                            DownloaderError{ ErrorLevel::FATAL, ErrorCode::PD_CURL, error });
                        break;
                    case CURLE_OPERATION_TIMEDOUT:
                        // Serious error
                        return tl::unexpected(
                            DownloaderError{ ErrorLevel::SERIOUS, ErrorCode::PD_CURL, error });
                        break;
                    default:
                        // Other error are not considered fatal
                        return tl::unexpected(
                            DownloaderError{ ErrorLevel::INFO, ErrorCode::PD_CURL, error });
                }
            }
        }

        // curl return code is CURLE_OK but we need to check status code
        curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &code);
        if (code)
        {
            char* effective_ip = NULL;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIMARY_IP, &effective_ip);

            // Check HTTP(S) code / or FTP
            if (code / 100 != 2)
            {
                return tl::unexpected(DownloaderError{
                    ErrorLevel::INFO,
                    ErrorCode::PD_CURL,
                    fmt::format(
                        "Status code: {} for {} (IP: {})", code, effective_url, effective_ip) });
            }
        }
        return {};
    }

    bool Downloader::is_max_mirrors_unlimited()
    {
        return max_mirrors_to_try <= 0;
    }

    tl::expected<std::shared_ptr<Mirror>, DownloaderError> Downloader::select_suitable_mirror(
        Target* target)
    {
        // This variable is used to indentify that all possible mirrors
        // were already tried and the transfer should be marked as failed.
        bool at_least_one_suitable_mirror_found = false;

        assert(target);

        if (target->mirrors.empty())
        {
            return tl::unexpected(DownloaderError{
                ErrorLevel::FATAL, ErrorCode::PD_MIRRORS, "No mirrors added for target" });
        }

        // mirrors_iterated is used to allow to use mirrors multiple times for a target
        std::size_t mirrors_iterated = 0;

        // retry local paths have no reason
        bool reiterate = false;

        // Iterate over mirrors for the target. If no suitable mirror is found on
        // the first iteration, relax the conditions (by allowing previously
        // failing mirrors to be used again) and do additional iterations up to
        // number of allowed failures equal to dd->allowed_mirror_failures.
        do
        {
            for (const auto& mirror : target->mirrors)
            {
                if (mirrors_iterated == 0)
                {
                    if (mirror->protocol != Protocol::kFILE)
                    {
                        reiterate = true;
                    }
                    if (target->tried_mirrors.count(mirror))
                    {
                        // This mirror was already tried for this target
                        continue;
                    }
                    if (mirror->successful_transfers == 0 && allowed_mirror_failures > 0
                        && mirror->failed_transfers >= allowed_mirror_failures)
                    {
                        // Skip bad mirrors
                        spdlog::info("Skipping bad mirror ({} failures and no success): {}",
                                     mirror->failed_transfers,
                                     mirror->url);
                    }
                }
                else if (mirror->protocol == Protocol::kFILE)
                {
                    // retry of local paths have no reason
                    continue;
                }
                else if (mirrors_iterated < mirror->failed_transfers)
                {
                    // On subsequent iterations, only skip mirrors that failed
                    // proportionally to the number of iterations. It allows to reuse
                    // mirrors with low number of failures first.
                    continue;
                }

                if (mirrors_iterated == 0 && mirror->protocol == Protocol::kFTP
                    && target->target->is_zchunk)
                {
                    continue;
                }

                // Skip each url that doesn't have "file://" or "file:" prefix
                if (ctx.offline && mirror->protocol != Protocol::kFILE)
                {
                    if (mirrors_iterated == 0)
                        spdlog::info("Skipping mirror {} - Offline mode enabled", mirror->url);
                    continue;
                }

                at_least_one_suitable_mirror_found = true;

                // Number of transfers which are downloading from the mirror
                // should always be lower or equal than maximum allowed number
                // of connection to a single host.
                if (mirror->allowed_parallel_connections > 0)
                {
                    assert(mirror->running_transfers <= mirror->allowed_parallel_connections);
                }

                // Check number of connections to the mirror
                if (mirror->is_parallel_connections_limited_and_reached())
                {
                    continue;
                }

                // This mirror looks suitable - use it
                return mirror;
            }
        } while (reiterate && target->retries < allowed_mirror_failures
                 && ++mirrors_iterated < allowed_mirror_failures);

        return tl::unexpected(DownloaderError(
            { ErrorLevel::FATAL,
              ErrorCode::PD_NOURL,
              fmt::format("No suitable mirror found for {}", target->target->complete_url) }));
    }

    // Select next target
    tl::expected<std::pair<Target*, std::string>, DownloaderError> Downloader::select_next_target()
    {
        Target* selected_target = nullptr;
        Mirror* mirror = nullptr;

        for (auto* target : m_targets)
        {
            std::shared_ptr<Mirror> mirror;
            std::string full_url;

            // Pick only waiting targets
            if (target->state != DownloadState::kWAITING)
                continue;

            // Determine if path is a complete URL
            bool complete_url_in_path = target->target->has_complete_url();

            bool have_mirrors = !target->mirrors.empty();
            // Sanity check
            if (target->target->base_url.empty() && !have_mirrors && !complete_url_in_path)
            {
                // Used relative path with empty internal mirrorlist and no basepath specified!
                return tl::unexpected(DownloaderError{
                    ErrorLevel::FATAL,
                    ErrorCode::PD_UNFINISHED,
                    "Empty mirrorlist and no basepath specified in DownloadTarget" });
            }

            // Prepare full target URL
            if (complete_url_in_path)
            {
                return std::make_pair(target, target->target->complete_url);
            }
            else
            {
                // Find a suitable mirror
                auto res = select_suitable_mirror(target);
                if (!res)
                {
                    target->call_end_callback(TransferStatus::kERROR);
                    return tl::unexpected(res.error());
                }

                mirror = res.value();

                assert(mirror);

                spdlog::info("Selected mirror: {}", mirror->format_url(target));
                if (mirror && !mirror->need_preparation(target))
                {
                    full_url = mirror->format_url(target);
                    target->mirror = mirror;
                }
                else
                {
                    // No free mirror
                    if (!mirror->need_preparation(target))
                    {
                        spdlog::info("Currently there is no free mirror for {}",
                                     target->target->path);
                    }
                }
            }

            // If LRO_OFFLINE is specified, check if the obtained full_url is local or not
            // This condition should never be true for a full_url built from a mirror, because
            // select_suitable_mirror() checks if the URL is local if LRO_OFFLINE is enabled by
            // itself.
            if (!full_url.empty() && ctx.offline && !starts_with(full_url, "file://"))
            {
                spdlog::info("Skipping {} because offline mode is active", full_url);

                // Mark the target as failed
                target->state = DownloadState::kFAILED;
                target->target->set_error(DownloaderError{
                    ErrorLevel::FATAL,
                    ErrorCode::PD_NOURL,
                    "Cannot download: offline mode is specified and no local URL is available." });

                auto cb_ret = target->call_end_callback(TransferStatus::kERROR);
                // TODO
                // if (cb_ret == CbReturnCode::kERROR || failfast)
                // {
                //     throw fatal_download_error(fmt::format(
                //         "Cannot download {}: Offline mode is specified and no local URL is
                //         available", target->target->path));
                // }
            }

            // A waiting target found
            if (mirror || !full_url.empty())
            {
                // Note: mirror is nullptr if base_url is used
                target->mirror = mirror;
                return std::make_pair(target, full_url);
            }
        }

        // No suitable target found
        return std::make_pair(nullptr, std::string());
    }

    bool Downloader::prepare_next_transfer(bool* candidate_found)
    {
        Protocol protocol = Protocol::kOTHER;

        *candidate_found = false;
        auto next_target = select_next_target();

        // Error
        if (!next_target)
            return false;

        auto [target, full_url] = next_target.value();

        if (!target)  // Nothing to do
            return true;

        if (target->mirror && target->mirror->need_wait_for_retry())
        {
            std::this_thread::sleep_until(target->mirror->next_retry);
        }

        *candidate_found = true;

        // Append the LRO_ONETIMEFLAG if instructed to do so
        // LrHandle *handle = target->handle;
        // if (handle && handle->onetimeflag && handle->onetimeflag_apply)
        // {
        //     const char *sep = "?";
        //     if (g_strrstr(full_url, sep) != NULL)
        //         sep = "&";
        //     char *new_url = g_strjoin(sep, full_url, handle->onetimeflag, NULL);
        //     g_free(full_url);
        //     full_url = new_url;
        //     // No other CURL handle on this LrHandle shall apply the flag again
        //     free(handle->onetimeflag);
        //     handle->onetimeflag = NULL;
        //     handle->onetimeflag_apply = FALSE;
        // }

        // protocol = lr_detect_protocol(full_url);
        protocol = Protocol::kHTTP;

        // Prepare CURL easy handle
        target->curl_handle.reset(new CURLHandle(ctx));
        CURLHandle& h = *(target->curl_handle);

        if (target->mirror && target->mirror->need_preparation(target))
        {
            target->mirror->prepare(target->target->path, h);
            target->state = DownloadState::kPREPARATION;

            CURLMcode cm_rc = curl_multi_add_handle(multi_handle, h);

            // Add the transfer to the list of running transfers
            m_running_transfers.push_back(target);

            return true;
        }

        // Set URL
        h.url(full_url);

        // Prepare FILE
#ifdef WITH_ZCHUNK
        if (!target->target->is_zchunk)
        {
#endif
            target->open_target_file();
            target->writecb_received = 0;
            target->writecb_required_range_written = false;
#ifdef WITH_ZCHUNK
        }
        // If file is zchunk, prep it
        if (target->target->is_zchunk)
        {
            if (!target->target->outfile)
            {
                spdlog::info("zck: opening file {}", target->temp_file.string());
                target->open_target_file();
                target->writecb_received = 0;
                target->writecb_required_range_written = false;
            }

            if (!check_zck(target))
            {
                spdlog::error("Unable to initialize zchunk file!");
                target->state = DownloadState::kFAILED;
                target->target->set_error(DownloaderError{
                    ErrorLevel::FATAL, ErrorCode::PD_ZCK, "Unable to initialize zchunk file" });
            }

            // If zchunk is finished, we're done, so move to next target
            if (target->zck_state == ZckState::kFINISHED)
            {
                spdlog::info("Target fully downloaded: {}", target->target->path);
                target->state = DownloadState::kFINISHED;
                target->reset();
                target->headercb_interrupt_reason.clear();
                target->call_end_callback(TransferStatus::kSUCCESSFUL);
                return prepare_next_transfer(candidate_found);
            }
        }
#endif /* WITH_ZCHUNK */

        if (target->resume && target->resume_count >= ctx.max_resume_count)
        {
            target->resume = false;
            spdlog::info("Download resume ignored, maximal number of attempts has been reached");
        }

        // Resume - set offset to resume incomplete download
        if (target->resume)
        {
            target->resume_count++;
            if (target->original_offset == -1)
            {
                // Determine offset
                target->target->outfile->seek(0L, SEEK_END);
                std::ptrdiff_t determined_offset = target->target->outfile->tell();

                if (determined_offset == -1)
                {
                    // An error while determining offset => download the whole file again
                    determined_offset = 0;
                }
                target->original_offset = determined_offset;
            }

            curl_off_t used_offset = target->original_offset;

            spdlog::info("Trying to resume from offset {}", used_offset);
            h.setopt(CURLOPT_RESUME_FROM_LARGE, used_offset);
        }

        if (target->target->byterange_start > 0)
        {
            assert(!target->target->resume && target->target->range.empty());
            h.setopt(CURLOPT_RESUME_FROM_LARGE, (curl_off_t) target->target->byterange_start);
        }

        // Set range if user specified one
        if (!target->target->range.empty())
        {
            assert(!target->target->resume && !target->target->byterange_start);
            h.setopt(CURLOPT_RANGE, target->target->range);
        }

        // Prepare progress callback
        target->callback_return_code = CbReturnCode::kOK;
        if (target->target->progress_callback)
        {
            h.setopt(CURLOPT_XFERINFOFUNCTION, &Target::progress_callback);
            h.setopt(CURLOPT_NOPROGRESS, 0);
            h.setopt(CURLOPT_XFERINFODATA, target);
        }

        // Prepare header callback
        if (target->target->expected_size > 0)
        {
            h.setopt(CURLOPT_HEADERFUNCTION, &Target::header_callback);
            h.setopt(CURLOPT_HEADERDATA, target);
        }

        // Prepare write callback
        h.setopt(CURLOPT_WRITEFUNCTION, &Target::write_callback);
        h.setopt(CURLOPT_WRITEDATA, target);

        // Set extra HTTP headers
        if (target->mirror)
        {
            h.add_headers(target->mirror->get_auth_headers(target->target->path));
        }

        h.add_headers(ctx.additional_httpheaders);

        if (target->target->no_cache)
        {
            // Add headers that tell proxy to serve us fresh data
            h.add_header("Cache-Control: no-cache");
            h.add_header("Pragma: no-cache");
        }

        // Add the new handle to the curl multi handle
        CURL* handle = h;
        CURLMcode cm_rc = curl_multi_add_handle(multi_handle, handle);
        assert(cm_rc == CURLM_OK);

        // Set the state of transfer as running
        target->state = DownloadState::kRUNNING;

        // Increase running transfers counter for mirror
        if (target->mirror)
        {
            target->mirror->increase_running_transfers();
        }

        // Set the state of header callback for this transfer
        target->headercb_state = HeaderCbState::kDEFAULT;
        target->headercb_interrupt_reason.clear();

        // Set protocol of the target
        target->protocol = protocol;

        // Add the transfer to the list of running transfers
        m_running_transfers.push_back(target);
        return true;
    }

    bool Downloader::prepare_next_transfers()
    {
        std::size_t length = m_running_transfers.size();
        std::size_t free_slots = max_parallel_connections - length;

        while (free_slots > 0)
        {
            bool candidate_found;
            if (!prepare_next_transfer(&candidate_found))
                return false;
            if (!candidate_found)
                break;
            free_slots--;
        }

        // Set maximal speed for each target
        if (!set_max_speeds_to_transfers())
            return false;

        return true;
    }

    /**
     * @brief Returns whether the download can be retried, using the same URL in
     * case of base_url or full path, or using another mirror in case of using
     * mirrors.
     *
     * @param complete_path_or_base_url determine type of download - mirrors or
     * base_url/fullpath
     * @return gboolean Return TRUE when another chance to download is allowed.
     */
    bool Downloader::can_retry_download(int num_of_tried_mirrors, const std::string& url)
    {
        if (!url.empty())
        {
            if (starts_with(url, "file:/"))
            {
                return false;
            }
            return allowed_mirror_failures > num_of_tried_mirrors;
        }
        // this means a mirror was used!
        return is_max_mirrors_unlimited() || num_of_tried_mirrors < max_mirrors_to_try;
    }

    bool Downloader::check_msgs(bool failfast)
    {
        int msgs_in_queue;
        CURLMsg* msg;
        while ((msg = curl_multi_info_read(multi_handle, &msgs_in_queue)))
        {
            if (msg->msg != CURLMSG_DONE)
            {
                // We are only interested in messages about finished transfers
                continue;
            }

            // TODO maybe refactor so that `msg` is passed to current target?
            Target* current_target = nullptr;
            for (auto* target : m_running_transfers)
            {
                if (target->curl_handle->ptr() == msg->easy_handle)
                {
                    current_target = target;
                    break;
                }
            }

            assert(current_target);

            char* tmp_effective_url;

            curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &tmp_effective_url);

            // Make the effective url persistent to survive the curl_easy_cleanup()
            std::string effective_url(tmp_effective_url);

            spdlog::info("Download finished {}", current_target->target->path);

            // Check status of finished transfer
            bool transfer_err = false, serious_err = false, fatal_err = false;
            bool fail_fast_err = false;

            auto result = check_finished_transfer_status(msg, current_target);
            if (!result)
            {
                transfer_err = true;
                serious_err = result.error().is_serious();
                fatal_err = result.error().is_fatal();
            }

            if (current_target->target->outfile && current_target->target->outfile->open())
            {
                current_target->target->outfile->flush();
            }

            if (transfer_err)
                goto transfer_error;

#ifdef WITH_ZCHUNK
            if (current_target->target->is_zchunk)
            {
                zckCtx* zck = nullptr;
                if (current_target->zck_state == ZckState::kHEADER_LEAD)
                {
                    if (!zck_read_lead(current_target))
                        goto transfer_error;
                }
                else if (current_target->zck_state == ZckState::kHEADER)
                {
                    if (current_target->mirror->max_ranges > 0
                        && current_target->mirror->protocol == Protocol::kHTTP
                        && !zck_valid_header(current_target))
                    {
                        goto transfer_error;
                    }
                }
                else if (current_target->zck_state == ZckState::kBODY)
                {
                    if (current_target->mirror->max_ranges > 0
                        && current_target->mirror->protocol == Protocol::kHTTP)
                    {
                        zckCtx* zck = zck_dl_get_zck(current_target->target->p_zck->zck_dl);
                        if (zck == nullptr)
                        {
                            spdlog::error("Unable to get zchunk file from download context");
                            result = tl::unexpected(DownloaderError{
                                ErrorLevel::SERIOUS,
                                ErrorCode::PD_ZCK,
                                "Unable to get zchunk file from download context" });
                            goto transfer_error;
                        }
                        if (zck_failed_chunks(zck) == 0 && zck_missing_chunks(zck) == 0)
                        {
                            current_target->zck_state = ZckState::kFINISHED;
                        }
                    }
                    else
                    {
                        current_target->zck_state = ZckState::kFINISHED;
                    }
                }
                if (current_target->zck_state == ZckState::kFINISHED)
                {
                    zck = zck_init_read(current_target);
                    if (!zck)
                        goto transfer_error;
                    if (zck_validate_checksums(zck) < 1)
                    {
                        zck_free(&zck);
                        spdlog::error("At least one of the zchunk checksums doesn't match in {}",
                                      effective_url);

                        result = tl::unexpected(DownloaderError{
                            ErrorLevel::SERIOUS,
                            ErrorCode::PD_BADCHECKSUM,
                            fmt::format("At least one of the zchunk checksums doesn't match in {}",
                                        effective_url) });

                        goto transfer_error;
                    }
                    zck_free(&zck);
                }
            }
            else
            {
#endif
                if (current_target->target->outfile)
                {
                    // New file was downloaded
                    if (!transfer_err && !current_target->check_filesize())
                    {
                        result = tl::unexpected(
                            DownloaderError({ ErrorLevel::SERIOUS,
                                              ErrorCode::PD_BADCHECKSUM,
                                              "Result file does not have expected filesize" }));
                        transfer_err = true;
                        goto transfer_error;
                    }
                    if (!transfer_err && !current_target->check_checksums())
                    {
                        result = tl::unexpected(
                            DownloaderError({ ErrorLevel::SERIOUS,
                                              ErrorCode::PD_BADCHECKSUM,
                                              "Result file does not have expected checksum" }));
                        transfer_err = true;
                        goto transfer_error;
                    }
                    if (!result)
                    {
                        current_target->reset_file(TransferStatus::kERROR);
                    }
                }
#ifdef WITH_ZCHUNK
            }
#endif

        transfer_error:
            // Cleanup
            curl_multi_remove_handle(multi_handle, current_target->curl_handle->ptr());

            // call_end_callback()
            if (!result)
            {
                result.error().log();
            }
            current_target->reset();

            current_target->headercb_interrupt_reason.clear();

            m_running_transfers.erase(
                std::find(m_running_transfers.begin(), m_running_transfers.end(), current_target));

            // TODO check if we were preparing here?
            current_target->tried_mirrors.insert(current_target->mirror);

            if (current_target->mirror)
            {
                bool success = transfer_err == false;
                current_target->mirror->update_statistics(success);
                if (ctx.adaptive_mirror_sorting)
                    sort_mirrors(current_target->mirrors,
                                 current_target->mirror,
                                 success,
                                 result.error().is_serious());
            }

            // There was an error during transfer
            if (!result)
            {
                // int complete_url_in_path = strstr(target->target->path, "://") ? 1 : 0;
                int complete_url_in_path = false;

                bool retry = false;

                spdlog::error("Error during transfer");

                // Call mirrorfailure callback
                // LrMirrorFailureCb mf_cb = target->target->mirrorfailurecb;
                // if (mf_cb)
                // {
                //     int rc = mf_cb(target->target->cbdata,
                //                    transfer_err->message,
                //                    effective_url);
                //     if (rc == LR_CB_ABORT)
                //     {
                //         // User wants to abort this download, so make the error fatal
                //         fatal_error = TRUE;
                //     }
                //     else if (rc == LR_CB_ERROR)
                //     {
                //         gchar *original_err_msg = g_strdup(transfer_err->message);
                //         g_clear_error(&transfer_err);
                //         g_info("Downloading was aborted by LR_CB_ERROR from "
                //                "mirror failure callback. Original error was: %s",
                //                original_err_msg);
                //         g_set_error(&transfer_err, LR_DOWNLOADER_ERROR,
                //         LRE_CBINTERRUPTED,
                //                     "Downloading was aborted by LR_CB_ERROR from "
                //                     "mirror failure callback. Original error was: "
                //                     "%s",
                //                     original_err_msg);
                //         g_free(original_err_msg);
                //         fatal_error = TRUE;
                //         target->callback_return_code = LR_CB_ERROR;
                //     }
                // }

                if (!result.error().is_fatal())
                {
                    // Temporary error (serious_error) during download occurred and
                    // another transfers are running or there are successful transfers
                    // and fewer failed transfers than tried parallel connections. It may be
                    // mirror is OK but accepts fewer parallel connections.
                    if (result.error().is_serious() && current_target->mirror
                        && (current_target->mirror->has_running_transfers()
                            || (current_target->mirror->successful_transfers > 0
                                && current_target->mirror->failed_transfers
                                       < current_target->mirror->max_tried_parallel_connections)))
                    {
                        spdlog::info("Lower maximum of parallel connections for mirror");
                        if (current_target->mirror->has_running_transfers())
                        {
                            current_target->mirror->set_allowed_parallel_connections(
                                current_target->mirror->running_transfers);
                        }
                        else
                        {
                            current_target->mirror->set_allowed_parallel_connections(1);
                        }

                        // Give used mirror another chance
                        current_target->tried_mirrors.erase(current_target->mirror);
                    }

                    // complete_url_in_path and target->base_url doesn't have an
                    // alternatives like using mirrors, therefore they are handled
                    // differently
                    std::string complete_url_or_base_url = complete_url_in_path
                                                               ? current_target->target->path
                                                               : current_target->target->base_url;
                    if (can_retry_download(static_cast<int>(current_target->retries),
                                           complete_url_or_base_url))
                    {
                        // Try another mirror or retry
                        if (!complete_url_or_base_url.empty())
                        {
                            spdlog::info("Ignore error - Retry download");
                        }
                        else
                        {
                            spdlog::info("Ignore error - Try another mirror");
                        }
                        current_target->state = DownloadState::kWAITING;
                        current_target->retries++;
                        retry = true;

                        // range fail
                        // if (status_code == 416)
                        // {
                        //     // if our resume file is too large we need to completely truncate it
                        //     current_target->original_offset = 0;
                        // }
#ifdef WITH_ZCHUNK
                        if (!current_target->target->is_zchunk
                            || current_target->zck_state == ZckState::kHEADER)
                        {
#endif
                            // Truncate file - remove downloaded garbage (error html page etc.)
                            if (!current_target->truncate_transfer_file())
                                return false;
#ifdef WITH_ZCHUNK
                        }
#endif
                    }
                }

                if (!retry)
                {
                    // No more mirrors to try or base_url used or fatal error
                    current_target->state = DownloadState::kFAILED;

                    // Call end callback
                    CbReturnCode rc = current_target->call_end_callback(TransferStatus::kERROR);
                    spdlog::error("Retries exceeded for {}", current_target->target->complete_url);

                    assert(!result);
                    current_target->target->set_error(result.error());

                    if (failfast || rc == CbReturnCode::kERROR)
                    {
                        // Fail fast is enabled, fail on any error
                        fail_fast_err = true;
                        spdlog::error("Failing fast or interrupted by error from end callback");
                    }
                    else
                    {
                        // Fail fast is disabled and callback doesn't report serious
                        // error, so this download is aborted, but other download
                        // can continue (do not abort whole downloading)
                    }
                }
            }
            else
            {
#ifdef WITH_ZCHUNK
                // No error encountered, transfer finished successfully
                if (current_target->target->is_zchunk
                    && current_target->zck_state != ZckState::kFINISHED)
                {
                    current_target->state = DownloadState::kWAITING;
                    current_target->tried_mirrors.erase(current_target->mirror);
                }
                else
                {
#endif /* WITH_ZCHUNK */
                    if (current_target->state == DownloadState::kRUNNING)
                    {
                        current_target->state = DownloadState::kFINISHED;
                    }
                    else if (current_target->state == DownloadState::kPREPARATION)
                    {
                        current_target->state = DownloadState::kWAITING;
                    }

                    // Remove xattr that states that the file is being downloaded
                    // by librepo, because the file is now completely downloaded
                    // and the xattr is not needed (is is useful only for resuming)
                    // remove_librepo_xattr(target->target);

                    current_target->curl_handle->finalize_transfer();

                    // only call the end callback if actually finished the download target
                    if (current_target->state == DownloadState::kFINISHED)
                    {
                        CbReturnCode rc
                            = current_target->call_end_callback(TransferStatus::kSUCCESSFUL);
                        if (rc == CbReturnCode::kERROR)
                        {
                            throw fatal_download_error("Interrupted by error from end callback");
                        }
                    }
#ifdef WITH_ZCHUNK
                }
#endif /* WITH_ZCHUNK */
                if (current_target->mirror)
                {
                    current_target->target->used_mirror = current_target->mirror;
                }

                current_target->target->effective_url = effective_url;
            }

            if (fail_fast_err)
            {
                // Interrupt whole downloading
                // A fatal error occurred or interrupted by callback
                return false;
            }
        }

        // At this point, after handles of finished transfers were removed
        // from the multi_handle, we could add new waiting transfers.
        return prepare_next_transfers();
    }

    bool Downloader::download()
    {
        int still_running, repeats = 0;
        const long max_wait_msecs = 1000;

        for (auto* target : m_targets)
        {
            if (target->target->already_downloaded())
            {
                spdlog::info("Found already downloaded file!");
                target->call_end_callback(TransferStatus::kALREADYEXISTS);
                target->state = DownloadState::kFINISHED;
            }
        }

        prepare_next_transfers();

        while (true)
        {
            CURLMcode code = curl_multi_perform(multi_handle, &still_running);

            if (code != CURLM_OK)
            {
                throw std::runtime_error(curl_multi_strerror(code));
            }
            bool check = check_msgs(failfast);
            if (!check)
            {
                curl_multi_cleanup(multi_handle);
                for (auto& target : m_running_transfers)
                {
                    target->target->set_error(DownloaderError{ ErrorLevel::FATAL,
                                                               ErrorCode::PD_INTERRUPTED,
                                                               "Download interrupted by error" });
                    target->call_end_callback(TransferStatus::kERROR);
                }
                return false;
            }

            if (!still_running && m_running_transfers.empty() && !is_sig_interrupted())
            {
                break;
            }

            long curl_timeout = -1;
            code = curl_multi_timeout(multi_handle, &curl_timeout);
            if (code != CURLM_OK)
            {
                throw std::runtime_error(curl_multi_strerror(code));
            }

            // No wait
            if (curl_timeout <= 0)
                continue;

            // Wait no more than 1s
            if (curl_timeout < 0 || curl_timeout > max_wait_msecs)
                curl_timeout = max_wait_msecs;

            int numfds;
            code = curl_multi_wait(multi_handle, NULL, 0, curl_timeout, &numfds);
            if (code != CURLM_OK)
            {
                throw std::runtime_error(curl_multi_strerror(code));
            }

            if (!numfds)
            {
                // count number of repeated zero numfds
                repeats++;
                if (repeats > 1)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            else
            {
                repeats = 0;
            }
        }

        spdlog::info("All downloads finished!");
        if (is_sig_interrupted())
        {
            spdlog::info("Download interrupted");
            curl_multi_cleanup(multi_handle);
            return false;
        }

        return true;
        // check if all targets are in finished state
        // finished = true;
        // for (auto& t : m_targets)
        // {
        //     if (t.state == DownloadState::kWAITING || t.state ==
        //     DownloadState::kRUNNING)
        //     {
        //         finished = false;
        //         break;
        //     }
        // }
    }

    // We do not implement the function of librepo to set a max speed per repository
    bool Downloader::set_max_speeds_to_transfers()
    {
        // Nothing to do
        if (m_running_transfers.empty() || ctx.max_speed_limit <= 0)
            return true;

        for (auto& current_target : m_running_transfers)
        {
            assert(ctx.max_speed_limit > 0);
            current_target->curl_handle->setopt(CURLOPT_MAX_RECV_SPEED_LARGE,
                                                (curl_off_t) ctx.max_speed_limit);
        }

        return true;
    }
}
