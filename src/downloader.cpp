#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>

extern "C"
{
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif
}

#include <spdlog/fmt/fmt.h>

namespace fs = std::filesystem;

#include "context.hpp"
#include "curl.hpp"
#include "download_target.hpp"
#include "downloader.hpp"
#include "enums.hpp"
#include "mirror.hpp"
#include "target.hpp"
#include "utils.hpp"
#ifdef WITH_ZCHUNK
#include "zck.hpp"
#endif
#include "result.hpp"

namespace powerloader
{
    Downloader::Downloader()
    {
        max_parallel_connections = 5;
        multi_handle = curl_multi_init();
        curl_multi_setopt(multi_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, max_parallel_connections);
    }

    Downloader::~Downloader()
    {
        curl_multi_cleanup(multi_handle);
    }

    void Downloader::add(DownloadTarget* dl_target)
    {
        // this function converts a DownloadTarget into a "Target"
        if (!dl_target)
            return;


        if (mirror_map.find(dl_target->base_url) != mirror_map.end())
        {
            m_targets.emplace_back(new Target(dl_target, mirror_map[dl_target->base_url]));
            dl_target->base_url.clear();
        }
        else
        {
            m_targets.emplace_back(new Target(dl_target));
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
    bool Downloader::check_finished_transfer_status(CURLMsg* msg, Target* target)
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
                throw download_error("Interrupted by header callback "
                                     + target->headercb_interrupt_reason);
            }
            // #ifdef WITH_ZCHUNK
            //             else if (target->range_fail)
            //             {
            //                 zckRange *range =
            //                 zck_dl_get_range(target->target->zck_dl); int range_count
            //                 = zck_get_range_count(range); if
            //                 (target->mirror->max_ranges >= range_count)
            //                 {
            //                     target->mirror->max_ranges = range_count / 2;
            //                     g_debug("%s: Setting mirror's max_ranges to %i",
            //                     __func__,
            //                             target->mirror->max_ranges);
            //                 }
            //                 return TRUE;
            //             }
            //             else if (target->target->zck_dl != NULL &&
            //             zck_is_error(zck_dl_get_zck(target->target->zck_dl)) > 0)
            //             {
            //                 zckCtx *zck = zck_dl_get_zck(target->target->zck_dl);

            //                 // Something went wrong while writing the zchunk file
            //                 g_set_error(transfer_err, LR_DOWNLOADER_ERROR, LRE_ZCK,
            //                             "Zchunk error: %s",
            //                             zck_get_error(zck));
            //                 if (zck_is_error(zck) == 1)
            //                 {
            //                     // Non-fatal zchunk error
            //                     g_info("Serious zchunk error: %s",
            //                            zck_get_error(zck));
            //                     *serious_error = TRUE;
            //                 }
            //                 else
            //                 { // zck_is_error(zck) == 2
            //                     // Fatal zchunk error
            //                     g_info("Fatal zchunk error: %s",
            //                            zck_get_error(zck));
            //                     *fatal_error = TRUE;
            //                 }
            //                 return TRUE;
            //             }
            // #endif /* WITH_ZCHUNK */
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
                        throw fatal_download_error(error);
                        break;
                    case CURLE_OPERATION_TIMEDOUT:
                        // Serious error
                        throw download_error(error, true);
                        break;
                    default:
                        // Other error are not considered fatal
                        throw download_error(error);
                }
            }
            return true;
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
                throw download_error(fmt::format(
                    "Status code: {} for {} (IP: {})", code, effective_url, effective_ip));
            }
        }
        target->finalize_success();
        return true;
    }

    bool Downloader::is_max_mirrors_unlimited()
    {
        return max_mirrors_to_try <= 0;
    }

    Mirror* Downloader::select_suitable_mirror(Target* target)
    {
        // This variable is used to indentify that all possible mirrors
        // were already tried and the transfer should be marked as failed.
        bool at_least_one_suitable_mirror_found = false;

        assert(target && target->mirrors);

        // mirrors_iterated is used to allow to use mirrors multiple times for a target
        std::size_t mirrors_iterated = 0;

        // retry local paths have no reason
        bool reiterate = false;

        //  Iterate over mirrors for the target. If no suitable mirror is found on
        //  the first iteration, relax the conditions (by allowing previously
        //  failing mirrors to be used again) and do additional iterations up to
        //  number of allowed failures equal to dd->allowed_mirror_failures.
        do
        {
            for (auto* mirror : *(target->mirrors))
            {
                if (mirrors_iterated == 0)
                {
                    if (mirror->protocol != Protocol::kFILE)
                    {
                        reiterate = true;
                    }
                    if (std::find(
                            target->tried_mirrors.begin(), target->tried_mirrors.end(), mirror)
                        != target->tried_mirrors.end())
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
                if (Context::instance().offline && mirror->protocol != Protocol::kFILE)
                {
                    if (mirrors_iterated == 0)
                        spdlog::info("Skipping mirror {} - Offline mode enabled", mirror->url);
                    continue;
                }

                at_least_one_suitable_mirror_found = true;

                // Number of transfers which are downloading from the mirror
                // should always be lower or equal than maximum allowed number
                // of connection to a single host.
                assert(max_connection_per_host == -1
                       || mirror->running_transfers <= max_connection_per_host);

                // Init max of allowed parallel connections from config
                mirror->init_once_allowed_parallel_connections(max_connection_per_host);

                // Check number of connections to the mirror
                if (mirror->is_parallel_connections_limited_and_reached())
                {
                    continue;
                }

                // This mirror looks suitable - use it
                return mirror;
            }
        } while (reiterate && target->tried_mirrors.size() < allowed_mirror_failures
                 && ++mirrors_iterated < allowed_mirror_failures);

        throw std::runtime_error(
            fmt::format("No suitable mirror found for {}", target->target->base_url));
    }

    // Select next target
    cpp::result<std::pair<Target*, std::string>, XError> Downloader::select_next_target()
    {
        // assert(selected_target);
        // assert(selected_full_url);

        Target* selected_target = nullptr;
        Mirror* mirror = nullptr;
        for (auto* target : m_targets)
        {
            Mirror* mirror = nullptr;
            std::string full_url;

            // Pick only waiting targets
            if (target->state != DownloadState::kWAITING)
                continue;

            // Determine if path is a complete URL
            bool complete_url_in_path = target->target->has_complete_url();

            bool have_mirrors = target->mirrors != nullptr && !target->mirrors->empty();
            // Sanity check
            if (target->target->base_url.empty() && !have_mirrors && !complete_url_in_path)
            {
                // Used relative path with empty internal mirrorlist and no basepath specified!
                return cpp::fail(
                    XError{ XError::FATAL,
                            "Empty mirrorlist and no basepath specified in DownloadTarget" });
            }

            // g_debug("Selecting mirror for: %s", target->target->path);

            // Prepare full target URL
            if (complete_url_in_path)
            {
                return std::make_pair(target, target->target->complete_url);
            }
            else
            {
                // Find a suitable mirror
                mirror = select_suitable_mirror(target);

                if (!mirror)
                {
                    return cpp::fail(XError{ XError::FATAL, "No mirror found" });
                }

                if (mirror && !mirror->need_preparation(target))
                {
                    full_url = mirror->format_url(target);
                }
                else
                {
                    // No free mirror
                    spdlog::info("Currently there is no free mirror for {}", target->target->path);
                }
            }

            // If LRO_OFFLINE is specified, check if the obtained full_url
            // is local or not
            // This condition should never be true for a full_url built
            // from a mirror, because select_suitable_mirror() checks if
            // the URL is local if LRO_OFFLINE is enabled by itself.
            if (!full_url.empty() && Context::instance().offline
                && !starts_with(full_url, "file://"))
            {
                spdlog::info("Skipping {} because OFFLINE is specified", full_url);

                // Mark the target as failed
                target->state = DownloadState::kFAILED;
                //     // lr_downloadtarget_set_error(target->target, LRE_NOURL,
                //     //                             "Cannot download, offline mode is
                //     specified and no "
                //     //                             "local URL is available");

                //     // Call end callback
                //     LrEndCb end_cb = target->target->endcb;
                //     if (end_cb)
                //     {
                //         int ret = end_cb(target->target->cbdata,
                //                          LR_TRANSFER_ERROR,
                //                          "Cannot download: Offline mode is specified "
                //                          "and no local URL is available");
                //         if (ret == LR_CB_ERROR)
                //         {
                //             target->cb_return_code = LR_CB_ERROR;
                //             g_debug("%s: Downloading was aborted by LR_CB_ERROR "
                //                     "from end callback",
                //                     __func__);
                //             g_set_error(err, LR_DOWNLOADER_ERROR, LRE_CBINTERRUPTED,
                //                         "Interrupted by LR_CB_ERROR from end callback");
                //             return false;
                //         }
                //     }

                if (failfast)
                {
                    throw fatal_download_error(fmt::format(
                        "Cannot download {}: Offline mode is specified and no local URL is available",
                        target->target->path));
                }
            }

            // A waiting target found
            if (mirror || !full_url.empty())
            {
                // Note: mirror is NULL if base_url is used
                target->mirror = mirror;
                return std::make_pair(target, full_url);
            }
        }

        // No suitable target found
        return std::make_pair(nullptr, std::string(""));
    }

    bool Downloader::prepare_next_transfer(bool* candidate_found)
    {
        Protocol protocol = Protocol::kOTHER;
        bool ret;

        *candidate_found = false;
        auto next_target = select_next_target();

        // Error
        if (next_target.has_error())
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
        target->curl_handle.reset(new CURLHandle());
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
        target->open_target_file();
        target->writecb_received = 0;
        target->writecb_required_range_written = false;

#ifdef WITH_ZCHUNK
        // If file is zchunk, prep it
        if (target->target->is_zchunk)
        {
            spdlog::info("opening {}", target->target->path);
            // TODO we need to open with a+ ... is there a better way?
            //      compare again with librepo
            target->f = fopen(target->target->path.c_str(), "a+");
            if (!check_zck(target))
            {
                spdlog::info("Could not initialize zchunk!");
                // g_set_error(err, LR_DOWNLOADER_ERROR, LRE_ZCK,
                //             "Unable to initialize zchunk file %s: %s",
                //             target->target->path,
                //             tmp_err->message);
                // TODO
                // goto fail;
            }

            // If zchunk is finished, we're done, so move to next target
            if (target->zck_state == ZckState::kFINISHED)
            {
                spdlog::info("Target already fully downloaded: {}", target->target->path);
                target->state = DownloadState::kFINISHED;
                target->reset();
                target->headercb_interrupt_reason.clear();
                // lr_downloadtarget_set_error(target->target, LRE_OK, NULL);
                return prepare_next_transfer(candidate_found);
            }
        }
#endif /* WITH_ZCHUNK */

        // int fd = fileno(target->f);

        // Allow resume only for files that were originally being
        // downloaded by librepo
        // if (target->resume && fs::exist(!is_resumable(target->target->fn, ))
        // {
        //     target->resume = false;
        //     p_debug("Resume ignored, existing file was not originally "
        //             "being downloaded by Librepo")
        //     if (ftruncate(fd, 0) == -1)
        //     {
        //         g_set_error(err, LR_DOWNLOADER_ERROR, LRE_IO,
        //                     "ftruncate() failed: %s", g_strerror(errno));
        //         goto fail;
        //     }
        // }

        if (target->resume && target->resume_count >= LR_DOWNLOADER_MAXIMAL_RESUME_COUNT)
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
                // fseek(target->f, 0L, SEEK_END);
                target->target->fd->seekp(0, std::ios::end);
                std::ptrdiff_t determined_offset = target->target->fd->tellp();

                if (determined_offset == -1)
                {
                    // An error while determining offset =>
                    // Download the whole file again
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
        target->cb_return_code = CbReturnCode::kOK;
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

        h.add_headers(Context::instance().additional_httpheaders);

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

    fail:
        // Cleanup target
        target->reset();
        return false;
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

            if (!current_target)
            {
                throw std::runtime_error("Could not find target associated with multi request");
            }

            char* tmp_effective_url;

            curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &tmp_effective_url);

            // Make the effective url persistent to survive the curl_easy_cleanup()
            std::string effective_url(tmp_effective_url);

            spdlog::info("Download finished {}", current_target->target->path);

            // Check status of finished transfer
            bool transfer_check = false;
            bool transfer_err = false, serious_err = false, fatal_err = false;
            bool fail_fast_err = false;

            try
            {
                transfer_check = check_finished_transfer_status(msg, current_target);
            }
            catch (const download_error& e)
            {
                std::cerr << "ERROR: " << e.what() << '\n';
                transfer_err = true;
                serious_err = e.serious;
                // non-fatal download error
            }
            catch (const fatal_download_error& e)
            {
                std::cerr << "ERROR: " << e.what() << '\n';
                transfer_err = true;
                fatal_err = true;
                // fatal download error
            }

            // Error
            // TODO!
            // if (!transfer_check)
            //     return false;

            if (current_target->f)
            {
                fflush(current_target->f);
                auto fd = fileno(current_target->f);
            }

        transfer_error:

            // Cleanup
            curl_multi_remove_handle(multi_handle, current_target->curl_handle->ptr());
            current_target->reset();

            current_target->headercb_interrupt_reason.clear();

            m_running_transfers.erase(
                std::find(m_running_transfers.begin(), m_running_transfers.end(), current_target));

            // TODO check if we were preparing here?
            current_target->tried_mirrors.insert(current_target->mirror);

            if (current_target->mirror)
            {
                bool success = transfer_err == false;
                // TODO
                current_target->mirror->update_statistics(success);
                if (Context::instance().adaptive_mirror_sorting)
                    sort_mirrors(
                        current_target->mirrors, current_target->mirror, success, serious_err);
            }

            // There was an error during transfer
            if (transfer_err)
            {
                // int complete_url_in_path = strstr(target->target->path, "://") ? 1 : 0;
                int complete_url_in_path = false;

                int num_of_tried_mirrors = current_target->tried_mirrors.size();
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
                //         target->cb_return_code = LR_CB_ERROR;
                //     }
                // }

                if (!fatal_err)
                {
                    // Temporary error (serious_error) during download occurred and
                    // another transfers are running or there are successful transfers
                    // and fewer failed transfers than tried parallel connections. It may be
                    // mirror is OK but accepts fewer parallel connections.
                    // TODO
                    // if (serious_error&& target->mirror &&
                    //     (has_running_transfers(target->mirror) ||
                    //      (target->mirror->successful_transfers > 0 &&
                    //       target->mirror->failed_transfers <
                    //       target->mirror->max_tried_parallel_connections)))
                    // {
                    //     g_debug("%s: Lower maximum of allowed parallel connections for
                    //     this mirror", __func__); if
                    //     (has_running_transfers(target->mirror))
                    //         target->mirror->allowed_parallel_connections =
                    //         target->mirror->running_transfers;
                    //     else
                    //         target->mirror->allowed_parallel_connections = 1;

                    //     // Give used mirror another chance
                    //     target->tried_mirrors = g_slist_remove(target->tried_mirrors,
                    //     target->mirror); num_of_tried_mirrors =
                    //     g_slist_length(target->tried_mirrors);
                    // }

                    // complete_url_in_path and target->base_url doesn't have an
                    // alternatives like using mirrors, therefore they are handled
                    // differently
                    // TODO
                    std::string complete_url_or_base_url = complete_url_in_path
                                                               ? current_target->target->path
                                                               : current_target->target->base_url;
                    if (can_retry_download(num_of_tried_mirrors, complete_url_or_base_url))
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
                        retry = true;

                        // range fail
                        // if (status_code == 416)
                        // {
                        //     // if our resume file is too large we need to completely truncate it
                        //     current_target->original_offset = 0;
                        // }
// Truncate file - remove downloaded garbage (error html page etc.)
#ifdef WITH_ZCHUNK
                        if (!current_target->target->is_zchunk
                            || current_target->zck_state == ZckState::kHEADER)
                        {
#endif
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
                    spdlog::info("No more retries (tried: {})", num_of_tried_mirrors);
                    current_target->state = DownloadState::kFAILED;

                    // Call end callback
                    CbReturnCode rc = current_target->call_endcallback(TransferStatus::kERROR);
                    // LrEndCb end_cb = target->target->endcb;
                    // if (end_cb)
                    // {
                    //     int rc = end_cb(target->target->cbdata,
                    //                     LR_TRANSFER_ERROR,
                    //                     transfer_err->message);
                    //     if (rc == LR_CB_ERROR)
                    //     {
                    //         target->cb_return_code = LR_CB_ERROR;
                    //         g_debug("%s: Downloading was aborted by LR_CB_ERROR "
                    //                 "from end callback",
                    //                 __func__);
                    //     }
                    // }

                    // lr_downloadtarget_set_error(target->target,
                    //                             transfer_err->code,
                    //                             "Download failed: %s",
                    //                             transfer_err->message);

                    if (failfast)
                    {
                        // Fail fast is enabled, fail on any error
                        throw fatal_download_error();
                        // g_propagate_error(&fail_fast_err, transfer_err);
                    }
                    // else if (current_target->cb_return_code == LR_CB_ERROR)
                    // {
                    //     // Callback returned LR_CB_ERROR, abort the downloading
                    //     g_debug("%s: Downloading was aborted by LR_CB_ERROR", __func__);
                    //     g_propagate_error(&fail_fast_err, transfer_err);
                    // }
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
                    // If we haven't finished downloading zchunk file, setup next
                    // download
                    current_target->state = DownloadState::kWAITING;
                    current_target->original_offset = -1;
                    // target->target->rcode = LRE_UNFINISHED;
                    // target->target->err = "Not finished";

                    // current_target->handle = target->target->handle;

                    current_target->tried_mirrors.erase(
                        std::find(current_target->tried_mirrors.begin(),
                                  current_target->tried_mirrors.end(),
                                  current_target->mirror));
                    // target->tried_mirrors = g_slist_remove(target->tried_mirrors,
                    // target->mirror);
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

                    // TODO current hack because of the memory management here ...
                    current_target->target->fd.reset();

                    // Remove xattr that states that the file is being downloaded
                    // by librepo, because the file is now completely downloaded
                    // and the xattr is not needed (is is useful only for resuming)
                    // remove_librepo_xattr(target->target);

                    // Call end callback
                    current_target->curl_handle->finalize_transfer();
                    EndCb end_cb = current_target->override_endcb ? current_target->override_endcb
                                                                  : current_target->target->endcb;
                    void* cb_data = current_target->override_endcb
                                        ? current_target->override_endcb_data
                                        : current_target->target->cbdata;
                    if (end_cb)
                    {
                        CbReturnCode rc = end_cb(TransferStatus::kSUCCESSFUL, "", cb_data);
                        // if (rc == LR_CB_ERROR)
                        // {
                        //     target->cb_return_code = LR_CB_ERROR;
                        //     g_debug("%s: Downloading was aborted by LR_CB_ERROR "
                        //             "from end callback",
                        //             __func__);
                        //     g_set_error(&fail_fast_err, LR_DOWNLOADER_ERROR,
                        //                 LRE_CBINTERRUPTED,
                        //                 "Interrupted by LR_CB_ERROR from end callback");
                        // }
                    }

                    // TODO?
                    // if (target->mirror)
                    //     lr_downloadtarget_set_usedmirror(target->target,
                    //                                      target->mirror->mirror->url);
#ifdef WITH_ZCHUNK
                }
#endif /* WITH_ZCHUNK */

                // TODO
                // lr_downloadtarget_set_error(target->target, LRE_OK, NULL);
                // lr_downloadtarget_set_effectiveurl(target->target,
                //                                    effective_url);
            }

            if (fail_fast_err)
            {
                // Interrupt whole downloading
                // A fatal error occurred or interrupted by callback
                // g_propagate_error(err, fail_fast_err);
                throw fatal_download_error();
                return false;
            }
        }

        // At this point, after handles of finished transfers were removed
        // from the multi_handle, we could add new waiting transfers.
        return prepare_next_transfers();

        // // Transfer was unsuccessful
        // if (transfer_err)
        // {
        //     goto transfer_error;
        // }

        // check checksums ...

        // preserve timestamp?

        // current_target->set_result(msg->data.result);
        // if (msg->data.result != CURLE_OK)
        // {
        //     if (current_target->can_retry())
        //     {
        //         curl_multi_remove_handle(multi_handle, current_target->handle());
        //         m_retry_targets.push_back(current_target);
        //         continue;
        //     }
        // }

        // if (msg->msg == CURLMSG_DONE)
        // {
        //     LOG_INFO << "Transfer done ...";
        //     // We are only interested in messages about finished transfers
        //     curl_multi_remove_handle(multi_handle, current_target->handle());

        //     // flush file & finalize transfer
        //     if (!current_target->finalize())
        //     {
        //         // transfer did not work! can we retry?
        //         if (current_target->can_retry())
        //         {
        //             LOG_INFO << "Adding target to retry!";
        //             m_retry_targets.push_back(current_target);
        //         }
        //         else
        //         {
        //             if (failfast && current_target->ignore_failure() == false)
        //             {
        //                 throw std::runtime_error("Multi-download failed.");
        //             }
        //         }
        //     }
        // }
        // }

        // prepare_next_transfers();

        return true;
    }

    void Downloader::download()
    {
        int still_running, repeats = 0;
        const long max_wait_msecs = 1000;
        prepare_next_transfers();

        while (true)
        {
            CURLMcode code = curl_multi_perform(multi_handle, &still_running);

            if (code != CURLM_OK)
            {
                throw std::runtime_error(curl_multi_strerror(code));
            }
            check_msgs(failfast);

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
            // return false;
            return;
        }

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

    bool Downloader::set_max_speeds_to_transfers()
    {
        // assert(!err || *err == NULL);

        // Nothing to do
        if (m_running_transfers.empty())
            return true;

        // Compute number of running downloads from repos with limited speed
        // GHashTable *num_running_downloads_per_repo = g_hash_table_new(NULL, NULL);
        // for (GSList *elem = dd->running_transfers; elem; elem = g_slist_next(elem)) {
        //     const LrTarget *ltarget = elem->data;

        //     if (!ltarget->handle || !ltarget->handle->maxspeed) // Skip repos with unlimited
        //     speed or without handle
        //         continue;

        //     guint num_running_downloads_from_repo =
        //         GPOINTER_TO_UINT(g_hash_table_lookup(num_running_downloads_per_repo,
        //         ltarget->handle));
        //     if (num_running_downloads_from_repo)
        //         ++num_running_downloads_from_repo;
        //     else
        //         num_running_downloads_from_repo = 1;
        //     g_hash_table_insert(num_running_downloads_per_repo, ltarget->handle,
        //                         GUINT_TO_POINTER(num_running_downloads_from_repo));
        // }

        // // Set max speed to transfers
        // GHashTableIter iter;
        // gpointer key, value;
        // g_hash_table_iter_init(&iter, num_running_downloads_per_repo);
        // while (g_hash_table_iter_next(&iter, &key, &value)) {
        //     const LrHandle *repo = key;
        //     const guint num_running_downloads_from_repo = GPOINTER_TO_UINT(value);

        //     // Calculate a max speed (rounded up) per target (for repo)
        //     const gint64 single_target_speed =
        //         (repo->maxspeed + (num_running_downloads_from_repo - 1)) /
        //         num_running_downloads_from_repo;

        //     for (GSList *elem = dd->running_transfers; elem; elem = g_slist_next(elem)) {
        //         LrTarget *ltarget = elem->data;
        //         if (ltarget->handle == repo) {
        //             CURL *curl_handle = ltarget->curl_handle;
        //             CURLcode code = curl_easy_setopt(curl_handle,
        //                                              CURLOPT_MAX_RECV_SPEED_LARGE,
        //                                              (curl_off_t)single_target_speed);
        //             if (code != CURLE_OK) {
        //                 g_set_error(err, LR_DOWNLOADER_ERROR, LRE_CURLSETOPT,
        //                             "Cannot set CURLOPT_MAX_RECV_SPEED_LARGE option: %s",
        //                             curl_easy_strerror(code));
        //                 g_hash_table_destroy(num_running_downloads_per_repo);
        //                 return FALSE;
        //             }
        //         }
        //     }
        // }

        // g_hash_table_destroy(num_running_downloads_per_repo);

        return true;
    }
}
