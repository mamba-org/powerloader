#ifndef POWERLOADER_CONTEXT_HPP
#define POWERLOADER_CONTEXT_HPP

#include <vector>
#include <string>
#include <chrono>
#include <map>
#include <filesystem>
#include <spdlog/spdlog.h>

#include <powerloader/export.hpp>


namespace powerloader
{
    namespace fs = std::filesystem;

    struct Mirror;

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

        using mirror_map_type = std::map<std::string, std::vector<std::shared_ptr<Mirror>>>;
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
