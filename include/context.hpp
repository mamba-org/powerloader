#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <map>

#ifdef WITH_ZCHUNK
extern "C"
{
#include <zck.h>
}
#endif
#include "mirror.hpp"

namespace powerloader
{
    class Context
    {
    public:
        bool offline = false;
        int verbosity = 0;
        bool adaptive_mirror_sorting = true;

        bool disable_ssl = false;

        long connect_timeout = 30L;
        long low_speed_time = 30L;
        long low_speed_limit = 1000L;
        bool ftp_use_seepsv = true;

        fs::path cache_dir;
        std::size_t retry_backoff_factor = 2;
        std::size_t max_resume_count = 3;
        std::chrono::steady_clock::duration retry_default_timeout = std::chrono::seconds(2);

        std::map<std::string, std::vector<std::shared_ptr<Mirror>>> mirror_map;

        std::vector<std::string> additional_httpheaders;

        static Context& instance();

        inline void set_verbosity(int v)
        {
            verbosity = v;
            if (v > 0)
            {
#ifdef WITH_ZCHUNK
                zck_set_log_level(ZCK_LOG_DEBUG);
#endif
                spdlog::set_level(spdlog::level::debug);
            }
            else
            {
                spdlog::set_level(spdlog::level::warn);
            }
        }

    private:
        Context();
        ~Context() = default;
    };

}
