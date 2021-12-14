#include <gtest/gtest.h>

#include "fileio.hpp"

using namespace powerloader;

TEST(fileio, open)
{
    std::error_code ec;
    FileIO f("test.txt", FileIO::write_update_binary, ec);

    EXPECT_FALSE(ec);
    f.write("test", 1, 4);
    f.close(ec);
    EXPECT_FALSE(ec);
}

TEST(fileio, truncate_empty)
{
    std::error_code ec;
    FileIO f("empty.txt", FileIO::append_update_binary, ec);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(fs::exists("empty.txt"));
    f.truncate(0, ec);
    EXPECT_FALSE(ec);
}
