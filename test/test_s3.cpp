#include <gtest/gtest.h>

#include <mirrors/s3.hpp>

using namespace powerloader;

TEST(s3, signdata)
{
    // S3Mirror::SignData s3mirror_signdata("GET", "");
    Context ctx;
    S3Mirror s3mirror_signdata(ctx, "someurl");

    const auto p0 = std::chrono::time_point<std::chrono::system_clock>{};

    auto s = s3mirror_signdata.calculate_signature(
        p0, "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY", "eu", "s3", "thisisateststring");

    EXPECT_EQ(s, "85ae731ab003e28b9d40bedf8f10967f43025942de2bae7dc99679c50a194457");

    auto s2 = s3mirror_signdata.calculate_signature(
        p0, "wXalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY", "eu", "s3", "thisisateststring");

    EXPECT_NE(s, s2);
    // s3mirror_signdata.add_extra_headers(nullptr);
}
