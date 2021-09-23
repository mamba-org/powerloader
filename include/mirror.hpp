#pragma once

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "curl.hpp"
#include "enums.hpp"
#include "utils.hpp"

#include "nlohmann/json.hpp"
#include <fmt/core.h>

class Target;

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
    Mirror(const std::string& url)
        : url(url)
        , preference(0)
        , protocol(Protocol::kHTTP)
    {
    }

    // URL of the mirror
    std::string url;
    // Integer number 1-100 - higher is better
    int preference;
    // Protocol of mirror (can be detected from URL)
    Protocol protocol;

    MirrorState state = MirrorState::READY;

    bool authenticated = false;

    std::chrono::steady_clock::time_point next_allowed_retry;

    // Maximum number of allowed parallel connections to this mirror. -1 means no
    // limit. Dynamically adjusted(decreased) if no fatal(temporary) error will
    // occur.
    int allowed_parallel_connections = 0;
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
    int max_ranges = 0;

    inline bool has_running_transfers()
    {
        return running_transfers > 0;
    }

    inline void init_once_allowed_parallel_connections(int max_allowed_parallel_connections)
    {
        if (allowed_parallel_connections == 0)
        {
            allowed_parallel_connections = max_allowed_parallel_connections;
        }
    }

    inline void increase_running_transfers()
    {
        running_transfers++;
        if (max_tried_parallel_connections < running_transfers)
        {
            max_tried_parallel_connections = running_transfers;
        }
    }

    inline bool is_parallel_connections_limited_and_reached()
    {
        return allowed_parallel_connections != -1
               && running_transfers >= allowed_parallel_connections;
    }

    inline void update_statistics(bool transfer_success)
    {
        running_transfers--;
        transfer_success ? successful_transfers++ : failed_transfers++;
    }

    virtual bool prepare(Target* target);
    virtual bool prepare(const std::string& path, CURLHandle& handle);

    virtual bool need_preparation(Target* target);
    virtual bool authenticate(CURLHandle& handle, const std::string& path)
    {
        return true;
    };

    virtual std::vector<std::string> get_auth_headers(const std::string& path);

    // virtual void add_extra_headers(Target* target) { return; };
    virtual std::string format_url(Target* target);
};
