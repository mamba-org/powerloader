#include <doctest/doctest.h>
#include <fstream>
#include <sstream>
#include <powerloader/fileio.hpp>

using namespace powerloader;

TEST_SUITE("fileio")
{
    TEST_CASE("open")
    {
        std::error_code ec;
        FileIO f("test.txt", FileIO::write_update_binary, ec);

        CHECK_FALSE(ec);
        f.write("test", 1, 4);
        f.close(ec);
        CHECK_FALSE(ec);
    }

    TEST_CASE("truncate_empty")
    {
        std::error_code ec;
        FileIO f("empty.txt", FileIO::append_update_binary, ec);
        CHECK_FALSE(ec);
        CHECK(fs::exists("empty.txt"));
        f.truncate(0, ec);
        CHECK_FALSE(ec);
    }

    TEST_CASE("replace_from")
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
            CHECK_EQ(f1.tell(), 6);
        }
        {
            std::ifstream f1("x1.txt");
            std::stringstream buffer;
            buffer << f1.rdbuf();
            CHECK_EQ(buffer.str(), "File 2");
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
            CHECK_EQ(f2.tell(), 33);
        }
        {
            std::ifstream f2("x2.txt");
            std::stringstream buffer;
            buffer << f2.rdbuf();
            CHECK_EQ(buffer.str(), "Hello world this is file number 1");
        }
    }
}
