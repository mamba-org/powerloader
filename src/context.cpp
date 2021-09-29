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
        set_verbosity(0);
    }

}
