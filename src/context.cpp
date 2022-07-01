#include <powerloader/context.hpp>

#include <exception>

#ifdef WITH_ZCHUNK
extern "C"
{
#include <zck.h>
}
#endif

namespace powerloader
{
    static std::atomic<bool> is_context_alive{ false };

    Context::Context()
    {
        bool expected = false;
        if (!is_context_alive.compare_exchange_strong(expected, true))
            throw std::runtime_error(
                "powerloader::Context created more than once - instance must be unique");

        cache_dir = fs::absolute(fs::path(".pdcache"));
        if (!fs::exists(cache_dir))
        {
            fs::create_directories(cache_dir);
        }
        set_verbosity(0);
    }

    Context::~Context()
    {
        is_context_alive = false;
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
