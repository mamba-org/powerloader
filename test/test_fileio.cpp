#include <gtest/gtest.h>

#include "fileio.hpp"
#include <fstream>

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

TEST(fileio, replace_from)
{
    std::error_code ec;
    {
        std::ofstream f1("x1.txt"), f2("x2.txt");
        f1 << "Hello world this is file number 1";
        f2 << "File 2";
    }
    {
        FileIO f1("x1.txt", FileIO::append_update_binary, ec);
        FileIO f2("x2.txt", FileIO::read_update_binary, ec);
        f1.replace_from(f2);

        f1.seek(0, SEEK_END);
        EXPECT_EQ(f1.tell(), 6);
    }
    {
        std::ifstream f1("x1.txt");
        std::stringstream buffer;
        buffer << f1.rdbuf();
        EXPECT_EQ(buffer.str(), "File 2");
    }
    {
        std::ofstream f1("x1.txt", std::ios::trunc), f2("x2.txt", std::ios::trunc);
        f1 << "Hello world this is file number 1";
        f2 << "File 2";
    }
    {
        FileIO f1("x1.txt", FileIO::read_update_binary, ec);
        FileIO f2("x2.txt", FileIO::append_update_binary, ec);
        f2.replace_from(f1);
        f2.seek(0, SEEK_END);
        EXPECT_EQ(f2.tell(), 33);
    }
    {
        std::ifstream f2("x2.txt");
        std::stringstream buffer;
        buffer << f2.rdbuf();
        EXPECT_EQ(buffer.str(), "Hello world this is file number 1");
    }
}
