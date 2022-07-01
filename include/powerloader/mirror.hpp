#ifndef PL_MIRROR_HPP
#define PL_MIRROR_HPP

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <spdlog/spdlog.h>

#include <powerloader/context.hpp>
#include <powerloader/curl.hpp>
#include <powerloader/enums.hpp>
#include <powerloader/utils.hpp>

#include "nlohmann/json.hpp"

namespace powerloader
{
    class Target;
    class Context;

    enum class MirrorState
    {
        WAITING,
        AUTHENTICATING,
        READY,
        RETRY_DELAY,
        AUTHENTICATION_FAILED,
        FAILED
    };

    // mirrors should be dict -> urls mapping
    struct Mirror
    {
        Mirror(const Context& ctx, const std::string& url);
        virtual ~Mirror() = default;

        Mirror(const Mirror&) = delete;
        Mirror& operator=(const Mirror&) = delete;
        Mirror(Mirror&&) = delete;
        Mirror& operator=(Mirror&&) = delete;

        bool need_wait_for_retry() const;
        bool has_running_transfers() const;

        void set_allowed_parallel_connections(int max_allowed_parallel_connections);
        void increase_running_transfers();

        bool is_parallel_connections_limited_and_reached() const;

        void update_statistics(bool transfer_success);

        // Return mirror rank or -1.0 if the rank cannot be determined
        // (e.g. when is too early)
        // Rank is currently just success rate for the mirror
        double rank() const;

        virtual bool prepare(Target* target);
        virtual bool prepare(const std::string& path, CURLHandle& handle);

        virtual bool need_preparation(Target* target);
        virtual bool authenticate(CURLHandle& handle, const std::string& path);

        virtual std::vector<std::string> get_auth_headers(const std::string& path) const;

        // virtual void add_extra_headers(Target* target) { return; };
        virtual std::string format_url(Target* target);

        // URL of the mirror
        std::string url;
        // Integer number 1-100 - higher is better
        int preference;
        // Protocol of mirror (can be detected from URL)
        Protocol protocol;

        MirrorState state = MirrorState::READY;

        bool authenticated = false;

        std::chrono::steady_clock::time_point next_allowed_retry;
        std::chrono::steady_clock::duration next_wait_duration;

        // Maximum number of allowed parallel connections to this mirror. -1 means no
        // limit. Dynamically adjusted(decreased) if no fatal(temporary) error will
        // occur.
        long allowed_parallel_connections = -1;
        // The maximum number of tried parallel connections to this mirror
        // (including unsuccessful).
        int max_tried_parallel_connections = 0;
        // How many transfers from this mirror are currently in progress.
        int running_transfers = 0;
        // How many transfers was finished successfully from the mirror.
        int successful_transfers = 0;
        // How many transfers failed.
        int failed_transfers = 0;
        // Maximum ranges supported in a single request.  This will be automatically
        // adjusted when mirrors respond with 200 to a range request
        int max_ranges = 256;

        // retry & backoff values
        std::chrono::system_clock::time_point next_retry;
        // first retry should wait for how many seconds?
        std::chrono::system_clock::duration retry_wait_seconds = std::chrono::milliseconds(200);
        // backoff factor for retry
        std::size_t retry_backoff_factor = 2;
        // count number of retries (this is not the same as failed transfers, as mutiple
        // transfers can be started at the same time, but should all be retried only once)
        std::size_t retry_counter = 0;
    };

    bool sort_mirrors(std::vector<std::shared_ptr<Mirror>>& mirrors,
                      const std::shared_ptr<Mirror>& mirror,
                      bool success,
                      bool serious);

}

#endif
