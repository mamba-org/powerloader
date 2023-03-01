#include <iostream>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

#include <powerloader/utils.hpp>

#include "curl_internal.hpp"

namespace powerloader
{
    namespace detail
    {
        struct InternalMirror
        {
            std::string url;
            CURLHandle handle;
            curl_off_t plain_connect_time;
        };

        long HALF_OF_SECOND_IN_MICROS = 500000;

        tl::expected<std::vector<std::string>, std::string> fastestmirror_perform(
            std::vector<InternalMirror>& mirrors, std::size_t length_of_measurement);
    }

    tl::expected<std::vector<std::string>, std::string> fastest_mirror(
        const Context& ctx, const std::vector<std::string>& urls)
    {
        std::vector<detail::InternalMirror> check_mirrors;
        for (const std::string& u : urls)
        {
            CURLHandle handle(ctx, u);
            handle.setopt(CURLOPT_CONNECT_ONLY, 1L);
            check_mirrors.push_back(detail::InternalMirror{ u, std::move(handle), -1 });
        }
        return fastestmirror_perform(check_mirrors, 1000000);
    }

    namespace detail
    {
        tl::expected<std::vector<std::string>, std::string> fastestmirror_perform(
            std::vector<detail::InternalMirror>& mirrors, std::size_t length_of_measurement)
        {
            if (mirrors.size() == 0)
                return {};

            CURLM* multihandle = curl_multi_init();
            if (!multihandle)
            {
                return tl::unexpected(std::string("curl_multi_init() error"));
            }

            // Add curl easy handles to multi handle
            std::size_t handles_added = 0;
            for (auto& el : mirrors)
            {
                if (el.handle.handle())
                {
                    curl_multi_add_handle(multihandle, el.handle.handle());
                    handles_added++;
                    spdlog::info("Checking URL: {}", el.url);
                }
            }

            if (handles_added == 0)
            {
                curl_multi_cleanup(multihandle);
                return {};
            }

            // cb(cbdata, LR_FMSTAGE_DETECTION, (void *) &handles_added);

            int still_running;
            // _cleanup_timer_destroy_ GTimer *timer = g_timer_new();
            // g_timer_start(timer);
            using time_point_t = std::chrono::steady_clock::time_point;
            time_point_t tend, tbegin = std::chrono::steady_clock::now();
            std::size_t elapsed_micros = 0;
            do
            {
                timeval timeout;
                int rc;
                CURLMcode cm_rc;
                int maxfd = -1;
                long curl_timeout = -1;
                fd_set fdread, fdwrite, fdexcep;

                FD_ZERO(&fdread);
                FD_ZERO(&fdwrite);
                FD_ZERO(&fdexcep);

                // Set suitable timeout to play around with
                timeout.tv_sec = 0;
                timeout.tv_usec = detail::HALF_OF_SECOND_IN_MICROS;

                cm_rc = curl_multi_timeout(multihandle, &curl_timeout);
                if (cm_rc != CURLM_OK)
                {
                    spdlog::error("fastestmirror: CURL multi failed");
                    curl_multi_cleanup(multihandle);
                    return tl::unexpected(
                        fmt::format("curl_multi_timeout() error: {}", curl_multi_strerror(cm_rc)));
                }

                // Set timeout to a reasonable value
                if (curl_timeout >= 0)
                {
                    timeout.tv_sec = curl_timeout / 1000;
                    if (timeout.tv_sec >= 1)
                    {
                        timeout.tv_sec = 0;
                        timeout.tv_usec = detail::HALF_OF_SECOND_IN_MICROS;
                    }
                    else
                    {
                        timeout.tv_usec = (curl_timeout % 1000) * 1000;
                        if (timeout.tv_usec > detail::HALF_OF_SECOND_IN_MICROS)
                            timeout.tv_usec = detail::HALF_OF_SECOND_IN_MICROS;
                    }
                }

                // Get file descriptors from the transfers
                cm_rc = curl_multi_fdset(multihandle, &fdread, &fdwrite, &fdexcep, &maxfd);
                if (cm_rc != CURLM_OK)
                {
                    spdlog::error("fastestmirror: CURL fd set error.");
                    curl_multi_cleanup(multihandle);
                    return tl::unexpected(
                        fmt::format("curl_multi_fdset() error: {}", curl_multi_strerror(cm_rc)));
                }

                rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
                if (rc < 0)
                {
                    if (errno == EINTR)
                    {
                        spdlog::debug("fastestmirror: select() interrupted by signal");
                    }
                    else
                    {
                        curl_multi_cleanup(multihandle);
                        return tl::unexpected(
                            fmt::format("select() error: %s", std::strerror(errno)));
                    }
                }

                curl_multi_perform(multihandle, &still_running);

                // Break loop after some reasonable amount of time
                tend = std::chrono::steady_clock::now();
                elapsed_micros
                    = std::chrono::duration_cast<std::chrono::microseconds>(tend - tbegin).count();
            } while (still_running && elapsed_micros < length_of_measurement);

            // Remove curl easy handles from multi handle and calculate plain_connect_time
            for (auto& el : mirrors)
            {
                // Remove handle
                curl_multi_remove_handle(multihandle, el.handle.handle());

                // Calculate plain_connect_time
                auto effective_url = el.handle.getinfo<std::string>(CURLINFO_EFFECTIVE_URL);

                if (!effective_url)
                {
                    // No effective url is most likely an error
                    el.plain_connect_time = std::numeric_limits<curl_off_t>::max();
                }
                else if (starts_with(effective_url.value(), "file:"))
                {
                    // Local directories are considered to be the best mirrors
                    el.plain_connect_time = 0;
                }
                else
                {
                    // Get connect time
                    curl_off_t namelookup_time
                        = el.handle.getinfo<curl_off_t>(CURLINFO_NAMELOOKUP_TIME_T).value_or(0);
                    curl_off_t connect_time
                        = el.handle.getinfo<curl_off_t>(CURLINFO_CONNECT_TIME_T).value_or(0);

                    if (connect_time == 0)
                    {
                        // Zero connect time is most likely an error
                        el.plain_connect_time = std::numeric_limits<curl_off_t>::max();
                    }
                    else
                    {
                        el.plain_connect_time = connect_time - namelookup_time;
                    }
                }
            }

            curl_multi_cleanup(multihandle);

            for (auto& el : mirrors)
            {
                spdlog::info("Mirror: {} -> {}", el.url, el.plain_connect_time);
            }

            // sort
            std::sort(mirrors.begin(),
                      mirrors.end(),
                      [](detail::InternalMirror& m1, detail::InternalMirror& m2)
                      { return m1.plain_connect_time < m2.plain_connect_time; });

            std::vector<std::string> sorted_urls(mirrors.size());
            for (auto& m : mirrors)
            {
                sorted_urls.push_back(m.url);
            }

            return sorted_urls;
        }
    }
}
