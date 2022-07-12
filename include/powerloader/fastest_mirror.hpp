#ifndef POWERLOADER_FASTEST_MIRROR_HPP
#define POWERLOADER_FASTEST_MIRROR_HPP

#include <string>
#include <vector>

#include <powerloader/export.hpp>

namespace powerloader
{
    class Context;

    POWERLOADER_API tl::expected<std::vector<std::string>, std::string> fastest_mirror(
        const Context& ctx, const std::vector<std::string>& urls);
}

#endif
