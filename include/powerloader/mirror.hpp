#ifndef POWERLOADER_MIRROR_HPP
#define POWERLOADER_MIRROR_HPP

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <powerloader/export.hpp>
#include <powerloader/context.hpp>
#include <powerloader/curl.hpp>
#include <powerloader/enums.hpp>
#include <powerloader/utils.hpp>
#include <powerloader/mirrorid.hpp>

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

    struct MirrorStats
    {
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

        // Returns the total count of finished transfered.
        int count_finished_transfers() const
        {
            return successful_transfers + failed_transfers;
        }
    };

    inline std::string strip_trailing_slash(const std::string& s)
    {
        if (s.size() > 0 && s.back() == '/' && s != "file://")
        {
            return s.substr(0, s.size() - 1);
        }
        return s;
    }

    // mirrors should be dict -> urls mapping
    class POWERLOADER_API Mirror
    {
    public:
        Mirror(const MirrorID& id, const Context& ctx, const std::string& url)
            : m_id(id)
            , m_url(strip_trailing_slash(url))
        {
            if (ctx.max_downloads_per_mirror > 0)
            {
                m_stats.allowed_parallel_connections = ctx.max_downloads_per_mirror;
            }
        }

        virtual ~Mirror();

        Mirror(const Mirror&) = delete;
        Mirror& operator=(const Mirror&) = delete;
        Mirror(Mirror&&) = delete;
        Mirror& operator=(Mirror&&) = delete;

        // Identifier used to compare mirror instances.
        const MirrorID& id() const
        {
            return m_id;
        }

        // URL of the mirror
        const std::string& url() const
        {
            return m_url;
        }

        // Protocol of mirror (can be detected from URL)
        Protocol protocol() const
        {
            return m_protocol;
        }

        // Statistics about this mirror.
        // TODO: consider returning by copy for concurrent safety... (like "capturing" the stats at
        // a given moment - but they might change while observing)
        const MirrorStats& stats() const
        {
            return m_stats;
        }

        void change_max_ranges(int new_value);

        std::chrono::system_clock::time_point next_retry() const
        {
            return m_next_retry;
        }

        // Return mirror rank or -1.0 if the rank cannot be determined
        // (e.g. when is too early)
        // Rank is currently just success rate for the mirror
        double rank() const;

        bool need_wait_for_retry() const;
        bool has_running_transfers() const;

        // Maximum number of allowed parallel connections to this mirror. -1 means no
        // limit. Dynamically adjusted(decreased) if no fatal(temporary) error will
        // occur.
        void set_allowed_parallel_connections(int max_allowed_parallel_connections);

        void increase_running_transfers();

        bool is_parallel_connections_limited_and_reached() const;

        void update_statistics(bool transfer_success);


        // TODO: protected: then  make this apply protection-against-change to these

        virtual bool prepare(Target* target);
        virtual bool prepare(const std::string& path, CURLHandle& handle);

        virtual bool needs_preparation(Target* target) const;
        virtual bool authenticate(CURLHandle& handle, const std::string& path);

        virtual std::vector<std::string> get_auth_headers(const std::string& path) const;

        // virtual void add_extra_headers(Target* target) { return; };
        virtual std::string format_url(Target* target) const;

        // TODO: use operator<=> instead once C++20 is enabled.
        [[nodiscard]] friend bool operator<(const Mirror& left, const Mirror& right)
        {
            return left.id() < right.id();
        }
        [[nodiscard]] friend bool operator==(const Mirror& left, const Mirror& right)
        {
            return left.id() == right.id();
        }

        static MirrorID id(const std::string& url)
        {
            return MirrorID(fmt::format("Mirror[{}]", url));
        }

    private:
        const MirrorID m_id;
        const std::string m_url;

        Protocol m_protocol = Protocol::kHTTP;
        MirrorState m_state = MirrorState::READY;

        std::chrono::steady_clock::time_point m_next_allowed_retry;

        MirrorStats m_stats;

        // retry & backoff values
        std::chrono::system_clock::time_point m_next_retry;

        // first retry should wait for how many seconds?
        std::chrono::system_clock::duration m_retry_wait_seconds = std::chrono::milliseconds(200);

        // backoff factor for retry
        std::size_t m_retry_backoff_factor = 2;

        // count number of retries (this is not the same as failed transfers, as mutiple
        // transfers can be started at the same time, but should all be retried only once)
        std::size_t m_retry_counter = 0;
    };

    class POWERLOADER_API HTTPMirror : public Mirror
    {
    public:
        HTTPMirror(const Context& ctx, const std::string& url)
            : Mirror(HTTPMirror::id(url), ctx, url)
        {
        }

        static MirrorID id(const std::string& url)
        {
            return MirrorID{ fmt::format("HTTPMirror[{}]", url) };
        }

        void set_auth(const std::string& user, const std::string& password);

        bool prepare(Target* target) override;
        bool prepare(const std::string& path, CURLHandle& handle) override;

        bool needs_preparation(Target* target) const override;
        bool authenticate(CURLHandle& handle, const std::string& path) override;

        std::vector<std::string> get_auth_headers(const std::string& path) const override;

        std::string format_url(Target* target) const override;

    private:
        std::string m_auth_user;
        std::string m_auth_password;
    };

    bool sort_mirrors(std::vector<std::shared_ptr<Mirror>>& mirrors,
                      const std::shared_ptr<Mirror>& mirror,
                      bool success,
                      bool serious);

}

#endif
