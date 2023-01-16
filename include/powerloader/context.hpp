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

    using mirror_set = std::vector<std::shared_ptr<Mirror>>; // TODO: replace by std::flat_set once available.
    using mirror_map_base = std::map<std::string, mirror_set>; // TODO: replace by std::flat_map once available.

    namespace details
    {
        bool already_exists(const MirrorID& id, const mirror_set& mirrors);
    }

    // TOOD: make this harder to get wrong by limiting insertion operations to only `add_unique_mirror`.
    class mirror_map_type : public mirror_map_base
    {
    public:
        using mirror_map_base::mirror_map_base;

        // Create, store and return a new instance of MirrorType created with `args` IFF no other
        // instance was created before with this type and arguments, returns null otherwise.
        template<typename MirrorType, typename... Args>
        auto add_unique_mirror(const std::string& host_name, Context& ctx, Args&&... args) // TODO: replace std::string by std::string_view as soon as a conversion is added.
            -> std::shared_ptr<MirrorType>
        {
            const auto new_id = MirrorID::make_id<MirrorType>(args...);
            auto& mirrors = (*this)[std::string(host_name)];
            if(details::already_exists(new_id, mirrors))
                return {};

            auto mirror = std::make_shared<MirrorType>(ctx, std::forward<Args>(args)...);
            mirrors.push_back(mirror);
            return mirror;
        }

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
