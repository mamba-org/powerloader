#include <gtest/gtest.h>

#include <powerloader/utils.hpp>

TEST(utility, erase_duplicates)
{
    auto values = std::vector<std::string>{
        "a", "a", "a","a", "b", "b", "c", "d", "d", "d"
    };
    const auto expected_values = std::vector<std::string>{
        "a", "b", "c", "d"
    };

    auto new_end = powerloader::erase_duplicates(values);
    EXPECT_EQ(new_end, values.end());
    EXPECT_EQ(values, expected_values);
}
