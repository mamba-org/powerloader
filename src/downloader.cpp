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
    Downloader::Downloader(const Context& lctx)
        : ctx(lctx)
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

        if (ctx.mirror_map.find(dl_target->base_url()) != ctx.mirror_map.end())
        {
            m_targets.emplace_back(
                new Target(ctx, dl_target, ctx.mirror_map.at(dl_target->base_url())));
            dl_target->clear_base_url();
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
            if (msg->data.result == CURLE_WRITE_ERROR && target->writecb_required_range_written())
            {
                // Download was interrupted by writecb because
                // user want only specified byte range of the
                // target and the range was already downloaded
                spdlog::info("Transfer was interrupted by writecb() because the required "
                             "range ({} - {}) was downloaded.",
                             target->target().byterange_start(),
                             target->target().byterange_end());
            }
            else if (target->headercb_state() == HeaderCbState::kINTERRUPTED)
            {
                // Download was interrupted by header callback
                return tl::unexpected(
                    DownloaderError{ ErrorLevel::FATAL,
                                     ErrorCode::PD_CBINTERRUPTED,
                                     fmt::format("Interrupted by header callback: {}",
                                                 target->headercb_interrupt_reason()) });
            }
#ifdef WITH_ZCHUNK
            else if (target->range_fail())
            {
                zckRange* range = zck_dl_get_range(target->target().zck().zck_dl);
                int range_count = zck_get_range_count(range);
                if (target->mirror()->stats().max_ranges >= range_count)
                {
                    target->mirror()->change_max_ranges(range_count / 2);
                    spdlog::debug("Setting mirror max_ranges to {}",
                                  target->mirror()->stats().max_ranges);
                }
            }
            else if (target->target().zck().zck_dl != nullptr
                     && zck_is_error(zck_dl_get_zck(target->target().zck().zck_dl)) > 0)
            {
                zckCtx* zck = zck_dl_get_zck(target->target().zck().zck_dl);

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
                                                target->errorbuffer());
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
            if (code / 100 != 2 && code != 304)
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
        assert(target);

        if (target->mirrors().empty())
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
            for (const auto& mirror : target->mirrors())
            {
                const auto mirror_stats = mirror->stats();
                if (mirrors_iterated == 0)
                {
                    if (mirror->protocol() != Protocol::kFILE)
                    {
                        reiterate = true;
                    }
                    if (target->tried_mirrors().count(mirror))
                    {
                        // This mirror was already tried for this target
                        continue;
                    }
                    if (mirror_stats.successful_transfers == 0 && allowed_mirror_failures > 0
                        && mirror_stats.failed_transfers >= allowed_mirror_failures)
                    {
                        // Skip bad mirrors
                        spdlog::info("Skipping bad mirror ({} failures and no success): {}",
                                     mirror_stats.failed_transfers,
                                     mirror->url());
                    }
                }
                else if (mirror->protocol() == Protocol::kFILE)
                {
                    // retry of local paths have no reason
                    continue;
                }
                else if (mirrors_iterated < static_cast<size_t>(mirror_stats.failed_transfers))
                {
                    // On subsequent iterations, only skip mirrors that failed
                    // proportionally to the number of iterations. It allows to reuse
                    // mirrors with low number of failures first.
                    continue;
                }

                if (mirrors_iterated == 0 && mirror->protocol() == Protocol::kFTP
                    && target->target().is_zchunck())
                {
                    continue;
                }

                // Skip each url that doesn't have "file://" or "file:" prefix
                if (ctx.offline && mirror->protocol() != Protocol::kFILE)
                {
                    if (mirrors_iterated == 0)
                        spdlog::info("Skipping mirror {} - Offline mode enabled", mirror->url());
                    continue;
                }

                // Number of transfers which are downloading from the mirror
                // should always be lower or equal than maximum allowed number
                // of connection to a single host.
                if (mirror_stats.allowed_parallel_connections > 0)
                {
                    // FIXME: either make this a proper error (exception or log) or remove this
                    // code? OR move the if condition in the assert?
                    assert(mirror_stats.running_transfers
                           <= mirror_stats.allowed_parallel_connections);
                }

                // Check number of connections to the mirror
                if (mirror->is_parallel_connections_limited_and_reached())
                {
                    continue;
                }

                // This mirror looks suitable - use it
                return mirror;
            }
        } while (reiterate && target->retries() < static_cast<std::size_t>(allowed_mirror_failures)
                 && ++mirrors_iterated < std::size_t(allowed_mirror_failures));

        return tl::unexpected(DownloaderError(
            { ErrorLevel::FATAL,
              ErrorCode::PD_NOURL,
              fmt::format("No suitable mirror found for {}", target->target().complete_url()) }));
    }

    // Select next target
    tl::expected<std::pair<Target*, std::string>, DownloaderError> Downloader::select_next_target()
    {
        for (auto* target : m_targets)
        {
            std::shared_ptr<Mirror> mirror;
            std::string full_url;

            // Pick only waiting targets
            if (target->state() != DownloadState::kWAITING)
                continue;

            // Determine if path is a complete URL
            bool complete_url_in_path = target->target().has_complete_url();

            bool have_mirrors = !target->mirrors().empty();
            // Sanity check
            if (target->target().base_url().empty() && !have_mirrors && !complete_url_in_path)
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
                full_url = target->target().complete_url();
            }
            else
            {
                // Find a suitable mirror
                auto res = select_suitable_mirror(target);
                if (!res)
                {
                    // TODO: review this: why is the callback called without changing the state
                    // of the target? (see Target::set_failed() for example).
                    target->call_end_callback(TransferStatus::kERROR);
                    return tl::unexpected(res.error());
                }

                mirror = res.value();

                assert(mirror);

                // TODO: create a `name()` or similar function
                spdlog::info("Selected mirror: {}", mirror->url());
                if (mirror && !mirror->needs_preparation(target))
                {
                    full_url = mirror->format_url(target);
                    target->change_mirror(mirror);
                }
                else
                {
                    // No free mirror
                    if (!mirror->needs_preparation(target))
                    {
                        spdlog::info("Currently there is no free mirror for {}",
                                     target->target().path());
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
                const auto cb_ret = target->set_failed(DownloaderError{
                    ErrorLevel::FATAL,
                    ErrorCode::PD_NOURL,
                    "Cannot download: offline mode is specified and no local URL is available." });

                // TODO
                // if (cb_ret == CbReturnCode::kERROR || failfast)
                // {
                //     throw fatal_download_error(fmt::format(
                //         "Cannot download {}: Offline mode is specified and no local URL is
                //         available", target->target().path()));
                // }
            }

            // A waiting target found
            if (mirror || !full_url.empty())
            {
                // Note: mirror is nullptr if base_url is used
                target->change_mirror(mirror);
                target->reset_response();
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

        if (target->mirror() && target->mirror()->need_wait_for_retry())
        {
            std::this_thread::sleep_until(target->mirror()->next_retry());
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

        target->prepare_for_transfer(multi_handle, full_url, protocol);

        if (target->zck_state() == ZckState::kFINISHED)
            return prepare_next_transfer(candidate_found);

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

    bool Downloader::check_msgs(bool lfailfast)
    {
        int msgs_in_queue;
        while (CURLMsg* msg = curl_multi_info_read(multi_handle, &msgs_in_queue))
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
                if (target->curl_handle() && target->curl_handle()->handle() == msg->easy_handle)
                {
                    current_target = target;
                    break;
                }
            }

            assert(current_target);

            char* tmp_effective_url = nullptr;

            curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &tmp_effective_url);

            // Make the effective url persistent to survive the curl_easy_cleanup()
            std::string effective_url(tmp_effective_url);

            spdlog::info("Download finished {}", current_target->target().path());

            // Check status of finished transfer
            bool transfer_err = false;
            bool fail_fast_err = false;

            auto result = check_finished_transfer_status(msg, current_target);
            if (!result)
            {
                transfer_err = true;
            }

            current_target->flush_target_file();

            if (!transfer_err)
            {
                result = current_target->finish_transfer(effective_url);
                transfer_err = !result;
            }

            // Cleanup
            curl_multi_remove_handle(multi_handle, current_target->curl_handle());

            // call_end_callback()
            if (!result)
            {
                result.error().log();
            }
            current_target->reset();

            m_running_transfers.erase(
                std::find(m_running_transfers.begin(), m_running_transfers.end(), current_target));

            // TODO: consider moving this call inside Target::finis_transfer()
            current_target->complete_mirror_usage(transfer_err == false, result);

            // There was an error during transfer
            if (!result)
            {
                // int complete_url_in_path = strstr(target->target().path(), "://") ? 1 : 0;
                int complete_url_in_path = false;

                bool retry = false;

                spdlog::error("Error during transfer");

                // Call mirrorfailure callback
                // LrMirrorFailureCb mf_cb = target->target().mirrorfailurecb;
                // if (mf_cb)
                // {
                //     int rc = mf_cb(target->target().cbdata,
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
                    if (result.error().is_serious()
                        && current_target->can_retry_transfer_with_fewer_connections())
                    {
                        spdlog::info("Lower maximum of parallel connections for mirror");
                        current_target->lower_mirror_parallel_connections();
                    }

                    // complete_url_in_path and target->base_url() doesn't have an
                    // alternatives like using mirrors, therefore they are handled
                    // differently
                    std::string complete_url_or_base_url
                        = complete_url_in_path ? current_target->target().path()
                                               : current_target->target().base_url();
                    if (can_retry_download(static_cast<int>(current_target->retries()),
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
                        retry = true;
                        const auto is_ready_to_retry = current_target->set_retrying();
                        if (!is_ready_to_retry)
                            return false;

                        // range fail
                        // if (status_code == 416)
                        // {
                        //     // if our resume file is too large we need to completely truncate it
                        //     current_target->original_offset = 0;
                        // }
                    }
                }

                if (!retry)
                {
                    // No more mirrors to try or base_url used or fatal error
                    spdlog::error("Retries exceeded for {}",
                                  current_target->target().complete_url());

                    assert(!result);
                    const CbReturnCode rc = current_target->set_failed(result.error());

                    if (lfailfast || rc == CbReturnCode::kERROR)
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
                // No error encountered, transfer finished successfully
                current_target->finalize_transfer(effective_url);
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
            target->check_if_already_finished();
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
                    target->set_failed(DownloaderError{ ErrorLevel::FATAL,
                                                        ErrorCode::PD_INTERRUPTED,
                                                        "Download interrupted by error" });
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
            current_target->set_to_max_speed();
        }

        return true;
    }
}
