#include <iostream>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

#include <powerloader/curl.hpp>
#include <powerloader/utils.hpp>

namespace powerloader
{
    namespace detail
    {
        struct InternalMirror
        {
            std::string url;
            CURL* handle;
            curl_off_t plain_connect_time;
        };

        bool fastestmirror_perform(std::vector<InternalMirror>& mirrors,
                                   std::size_t length_of_measurement);
    }

    namespace
    {
        long HALF_OF_SECOND_IN_MICROS = 500000;
    }

    void fastest_mirror(const Context& ctx, const std::vector<std::string>& urls)
    {
        std::vector<detail::InternalMirror> check_mirrors;
        for (const std::string& u : urls)
        {
            CURL* handle = get_handle(ctx);

            int curlcode = curl_easy_setopt(handle, CURLOPT_URL, u.c_str());
            if (curlcode != CURLE_OK)
            {
                // g_set_error(err, LR_FASTESTMIRROR_ERROR, LRE_CURL,
                //             "curl_easy_setopt(_, CURLOPT_URL, %s) failed: %s",
                //             url, curl_easy_strerror(curlcode));
                // curl_easy_cleanup(curlh);
                // ret = FALSE;
                // break;
            }

            curlcode = curl_easy_setopt(handle, CURLOPT_CONNECT_ONLY, 1);
            if (curlcode != CURLE_OK)
            {
                // g_set_error(err, LR_FASTESTMIRROR_ERROR, LRE_CURL,
                //         "curl_easy_setopt(_, CURLOPT_CONNECT_ONLY, 1) failed: %s",
                //         curl_easy_strerror(curlcode));
                // curl_easy_cleanup(curlh);
                // ret = FALSE;
                // break;
            }

            detail::InternalMirror im{ u, handle, -1 };
            check_mirrors.push_back(im);
        }
        fastestmirror_perform(check_mirrors, 1000000);
    }

    namespace detail
    {
        bool fastestmirror_perform(std::vector<detail::InternalMirror>& mirrors,
                                   std::size_t length_of_measurement)
        {
            if (mirrors.size() == 0)
                return false;

            CURLM* multihandle = curl_multi_init();
            if (!multihandle)
            {
                // g_set_error(err, LR_FASTESTMIRROR_ERROR, LRE_CURL,
                //             "curl_multi_init() error");
                return false;
            }

            // Add curl easy handles to multi handle
            std::size_t handles_added = 0;
            for (auto& el : mirrors)
            {
                if (el.handle)
                {
                    curl_multi_add_handle(multihandle, el.handle);
                    handles_added++;
                    std::cout << "Checking el.url " << el.url << std::endl;
                }
            }

            if (handles_added == 0)
            {
                curl_multi_cleanup(multihandle);
                return true;
            }

            // cb(cbdata, LR_FMSTAGE_DETECTION, (void *) &handles_added);

            int still_running;
            double elapsed_time = 0.0;
            // _cleanup_timer_destroy_ GTimer *timer = g_timer_new();
            // g_timer_start(timer);

            std::chrono::steady_clock::time_point tbegin = std::chrono::steady_clock::now();
            std::chrono::steady_clock::time_point tend = std::chrono::steady_clock::now();
            std::size_t elapsed_micros = 0;
            do
            {
                timeval timeout;
                int rc, cm_rc;
                int maxfd = -1;
                long curl_timeout = -1;
                fd_set fdread, fdwrite, fdexcep;

                FD_ZERO(&fdread);
                FD_ZERO(&fdwrite);
                FD_ZERO(&fdexcep);

                // Set suitable timeout to play around with
                timeout.tv_sec = 0;
                timeout.tv_usec = HALF_OF_SECOND_IN_MICROS;

                cm_rc = curl_multi_timeout(multihandle, &curl_timeout);
                if (cm_rc != CURLM_OK)
                {
                    spdlog::error("curl multi failed");
                    // g_set_error(err, LR_FASTESTMIRROR_ERROR, LRE_CURLM,
                    //             "curl_multi_timeout() error: %s",
                    //             curl_multi_strerror(cm_rc));
                    curl_multi_cleanup(multihandle);
                    return false;
                }

                // Set timeout to a reasonable value
                if (curl_timeout >= 0)
                {
                    timeout.tv_sec = curl_timeout / 1000;
                    if (timeout.tv_sec >= 1)
                    {
                        timeout.tv_sec = 0;
                        timeout.tv_usec = HALF_OF_SECOND_IN_MICROS;
                    }
                    else
                    {
                        timeout.tv_usec = (curl_timeout % 1000) * 1000;
                        if (timeout.tv_usec > HALF_OF_SECOND_IN_MICROS)
                            timeout.tv_usec = HALF_OF_SECOND_IN_MICROS;
                    }
                }

                // Get file descriptors from the transfers
                cm_rc = curl_multi_fdset(multihandle, &fdread, &fdwrite, &fdexcep, &maxfd);
                if (cm_rc != CURLM_OK)
                {
                    std::cout << "curl FD SET error" << std::endl;
                    // g_set_error(err, LR_FASTESTMIRROR_ERROR, LRE_CURLM,
                    //             "curl_multi_fdset() error: %s",
                    //             curl_multi_strerror(cm_rc));
                    curl_multi_cleanup(multihandle);
                    return false;
                }

                rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
                if (rc < 0)
                {
                    if (errno == EINTR)
                    {
                        // std::cout << ("%s: select() interrupted by signal", __func__);
                    }
                    else
                    {
                        // g_set_error(err, LR_FASTESTMIRROR_ERROR, LRE_SELECT,
                        //             "select() error: %s", g_strerror(errno));
                        // std::cout << "EROOR" << std::endl;
                        curl_multi_cleanup(multihandle);
                        return false;
                    }
                }

                curl_multi_perform(multihandle, &still_running);

                // Break loop after some reasonable amount of time

                tend = std::chrono::steady_clock::now();
                elapsed_micros
                    = std::chrono::duration_cast<std::chrono::microseconds>(tend - tbegin).count();
                // elapsed_time = g_timer_elapsed(timer, NULL);
            } while (still_running && elapsed_micros < length_of_measurement);

            // Remove curl easy handles from multi handle
            // and calculate plain_connect_time
            for (auto& el : mirrors)
            {
                CURL* handle = el.handle;
                if (!handle)
                    continue;

                // Remove handle
                curl_multi_remove_handle(multihandle, handle);

                // Calculate plain_connect_time
                char* effective_url;
                curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &effective_url);

                if (!effective_url)
                {
                    // No effective url is most likely an error
                    el.plain_connect_time = std::numeric_limits<curl_off_t>::max();
                }
                else if (starts_with(effective_url, "file:"))
                {
                    // Local directories are considered to be the best mirrors
                    el.plain_connect_time = 0;
                }
                else
                {
                    // Get connect time
                    curl_off_t namelookup_time;
                    curl_off_t connect_time;
                    curl_easy_getinfo(handle, CURLINFO_NAMELOOKUP_TIME_T, &namelookup_time);
                    curl_easy_getinfo(handle, CURLINFO_CONNECT_TIME_T, &connect_time);

                    if (connect_time == 0)
                    {
                        // Zero connect time is most likely an error
                        el.plain_connect_time = std::numeric_limits<std::size_t>::max();
                    }
                    else
                    {
                        el.plain_connect_time = connect_time - namelookup_time;
                    }

                    // g_debug("%s: name_lookup: %3.6f connect_time:  %3.6f (%3.6f) | %s",
                    //        __func__, namelookup_time, connect_time,
                    //        mirror->plain_connect_time, mirror->url);
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
            return true;
        }
    }

}
