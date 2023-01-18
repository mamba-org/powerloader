#include <doctest/doctest.h>

#include <powerloader/utils.hpp>

TEST_SUITE("utility")
{
    TEST_CASE("erase_duplicates")
    {
        auto values = std::vector<std::string>{ "a", "a", "a", "a", "b", "b", "c", "d", "d", "d" };
        const auto expected_values = std::vector<std::string>{ "a", "b", "c", "d" };

        auto new_end = powerloader::erase_duplicates(values);
        CHECK_EQ(new_end, values.end());
        CHECK_EQ(values, expected_values);
    }
}
