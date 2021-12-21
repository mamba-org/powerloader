#include <gtest/gtest.h>
#include <fstream>
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

TEST(fileio, replace_contents)
{
    std::error_code ec;
    FileIO f1("f1.txt", FileIO::write_update_binary, ec);
    EXPECT_FALSE(ec);
    FileIO f2("f2.txt", FileIO::write_update_binary, ec);
    EXPECT_FALSE(ec);
    FileIO f3("f3.txt", FileIO::write_update_binary, ec);
    EXPECT_FALSE(ec);

    f1.write("hello world", 1, 12);
    f2.replace_contents_with(f1, ec);
    EXPECT_FALSE(ec);
    EXPECT_EQ(f2.seek(0, SEEK_END), 0);
    EXPECT_EQ(f2.tell(), 12);

    std::ifstream x2("f2.txt");
    std::stringstream buffer;
    buffer << x2.rdbuf();

    EXPECT_EQ(buffer.str(), std::string("hello world\0", 12));

    f3.write("test", 1, 5);
    f2.replace_contents_with(f3, ec);
    EXPECT_FALSE(ec);
    EXPECT_EQ(f2.seek(0, SEEK_END), 0);
    EXPECT_EQ(f2.tell(), 5);

    std::ifstream x22("f2.txt");
    std::stringstream buffer2;
    buffer2 << x22.rdbuf();

    EXPECT_EQ(buffer2.str(), std::string("test\0", 5));
}
