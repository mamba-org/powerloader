#ifndef POWERLOADER_CONTEXT_HPP
#define POWERLOADER_CONTEXT_HPP

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <map>
#include <filesystem>
#include <spdlog/spdlog.h>

#include <powerloader/export.hpp>
#include <powerloader/mirrorid.hpp>

namespace powerloader
{
    namespace fs = std::filesystem;

    class Context;
    struct Mirror;

    using mirror_set
        = std::vector<std::shared_ptr<Mirror>>;  // TODO: replace by std::flat_set once available.
    using mirror_map_base
        = std::map<std::string, mirror_set>;  // TODO: replace by std::flat_map once available.

    namespace details
    {
        bool already_exists(const MirrorID& id, const mirror_set& mirrors);
        bool is_every_mirror_unique_per_host(const mirror_map_base& mirrors);
    }

    // Registry of (host name -> list of mirrors) which guarantee that every list
    // of mirror have a unique set of mirrors (no duplicates).
    class mirror_map_type : private mirror_map_base
    {
    public:
        using mirror_map_base::begin;
        using mirror_map_base::clear;
        using mirror_map_base::end;
        using mirror_map_base::empty;
        using mirror_map_base::mirror_map_base;
        using mirror_map_base::size;

        // Get a list of unique mirorrs if existing for the provided host name, or an empty list
        // otherwise.
        mirror_set get_mirrors(std::string_view host_name) const;

        // Returns a copy of this container's values in the shape of a map.
        mirror_map_base as_map() const
        {
            return *this;
        }

        // Returns true if there are registered mirrors stored here, false if none are.
        bool has_mirrors(std::string_view host_name) const;

        // Creates, stores and return a new instance of `MirrorType` created with `args` IFF no
        // other mirror is already registed with the same id for the specified host, returns null
        // otherwise.
        template <typename MirrorType, typename... Args>
        auto create_unique_mirror(std::string_view host_name, const Context& ctx, Args&&... args)
            -> std::shared_ptr<MirrorType>
        {
            static_assert(std::is_base_of_v<Mirror, MirrorType>);

            const auto new_id = MirrorID::make_id<MirrorType>(args...);
            auto& mirrors = (*this)[std::string(host_name)];
            if (details::already_exists(new_id, mirrors))
                return {};

            auto mirror = std::make_shared<MirrorType>(ctx, std::forward<Args>(args)...);
            mirrors.push_back(mirror);
            return mirror;
        }

        // Stores a provided Mirror IFF no other mirror is already registed with the same id for the
        // specified host. Returns true if the mirror has been stored, false otherwise.
        bool add_unique_mirror(std::string_view host_name, std::shared_ptr<Mirror> mirror);

        // Reset the whole mapping to a new set of host -> mirrors values.
        // Without arguments, this clears all values.
        // Every `mirror_set` in `new_values` must have no duplicates mirrors for that set,
        // otherwise this will throw a `std::invalid_argument` exception.
        void reset(mirror_map_base new_values = {});
    };

    class POWERLOADER_API Context
    {
    public:
        bool offline = false;
        int verbosity = 0;
        bool adaptive_mirror_sorting = true;

        // ssl options
        bool disable_ssl = false;
        bool ssl_no_revoke = false;
        fs::path ssl_ca_info;
        int ssl_backend = -1;

        bool validate_checksum = true;

        long connect_timeout = 30L;
        long low_speed_time = 30L;
        long low_speed_limit = 1000L;

        long max_speed_limit = -1L;
        long max_parallel_downloads = 5L;
        long max_downloads_per_mirror = -1L;

        // This can improve throughput significantly
        // see https://github.com/curl/curl/issues/9601
        long transfer_buffersize = 100 * 1024;

        bool preserve_filetime = true;
        bool ftp_use_seepsv = true;

        fs::path cache_dir;
        std::size_t retry_backoff_factor = 2;
        std::size_t max_resume_count = 3;
        std::chrono::steady_clock::duration retry_default_timeout = std::chrono::seconds(2);

        mirror_map_type mirror_map;

        std::vector<std::string> additional_httpheaders;

        void set_verbosity(int v);

        // Throws if another instance already exists: there can only be one at any time!
        Context();
        ~Context();

        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;
        Context(Context&&) = delete;
        Context& operator=(Context&&) = delete;
    };

}

#endif
