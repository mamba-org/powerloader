#ifndef POWERLOADER_MIRRORID_HPP
#define POWERLOADER_MIRRORID_HPP

#include <string>
#include <fmt/format.h>

namespace powerloader
{

    // Identifies a Mirror and is used to compare Mirrors.
    class MirrorID
    {
        std::string value;

    public:
        MirrorID() = default;
        MirrorID(const MirrorID&) = default;
        MirrorID& operator=(const MirrorID&) = default;
        MirrorID(MirrorID&&) = default;
        MirrorID& operator=(MirrorID&&) = default;

        explicit MirrorID(const std::string& v)
            : value(v)
        {
        }

        std::string to_string() const
        {
            return fmt::format("MirrorID <{}>", value);
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
