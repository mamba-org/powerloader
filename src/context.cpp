#include "context.hpp"

#ifdef WITH_ZCHUNK
extern "C"
{
#include <zck.h>
}
#endif

namespace powerloader
{
    Context& Context::instance()
    {
        static Context ctx;
        return ctx;
    }

    Context::Context()
    {
        cache_dir = fs::absolute(fs::path(".pdcache"));
        if (!fs::exists(cache_dir))
        {
            fs::create_directories(cache_dir);
        }
        set_verbosity(0);
    }

    void Context::set_verbosity(int v)
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
}
