#include <gtest/gtest.h>

#include "fileio.hpp"

using namespace powerloader;

TEST(fileio, open)
{
    std::error_code ec;
    FileIO f("test.txt", "w+b", ec);
    EXPECT_FALSE(ec);
    f.write("test", 1, 4);
    f.close(ec);
    EXPECT_FALSE(ec);
}

TEST(fileio, truncate_empty)
{
    std::error_code ec;
    FileIO f("empty.txt", "a+b", ec);
    EXPECT_FALSE(ec);
    f.truncate(0, ec);
    EXPECT_FALSE(ec);
}
