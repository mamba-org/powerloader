#include "context.hpp"

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

}
