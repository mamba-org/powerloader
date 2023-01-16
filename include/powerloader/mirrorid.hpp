#ifndef POWERLOADER_MIRRORID_HPP
#define POWERLOADER_MIRRORID_HPP

#include <string>
#include <fmt/format.h>

namespace powerloader
{

    // Identifies a Mirror and is used to compare Mirrors.
    struct MirrorID
    {
        std::string value;

        template <typename MirrorType, typename... StringType>
        static MirrorID make_id(StringType&&... args)
        {
            return { std::string{} + fmt::format("{}:", typeid(MirrorType).name())
                     + (fmt::format("{},", args) + ...) };
        }

        // TODO: use operator<=> instead once C++20 is enabled.
        [[nodiscard]] friend bool operator<(const MirrorID& left, const MirrorID& right)
        {
            return left.value < right.value;
        }
        [[nodiscard]] friend bool operator==(const MirrorID& left, const MirrorID& right)
        {
            return left.value == right.value;
        }
    };
}


#endif
