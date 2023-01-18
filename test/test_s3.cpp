#include <doctest/doctest.h>

#include <powerloader/mirrors/s3.hpp>

using namespace powerloader;

TEST_SUITE("s3")
{
    TEST_CASE("signdata")
    {
        const auto p0 = std::chrono::time_point<std::chrono::system_clock>{};

        const auto s = s3_calculate_signature(
            p0, "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY", "eu", "s3", "thisisateststring");

        CHECK_EQ(s, "85ae731ab003e28b9d40bedf8f10967f43025942de2bae7dc99679c50a194457");

        const auto s2 = s3_calculate_signature(
            p0, "wXalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY", "eu", "s3", "thisisateststring");

        CHECK_NE(s, s2);
        // s3mirror_signdata.add_extra_headers(nullptr);
    }
}
// TODO: add tests actually usuing S3Mirror
