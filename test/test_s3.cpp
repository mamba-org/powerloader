#include <gtest/gtest.h>

#include "mirrors/s3.hpp"

// S3 data we need:
// we can assume a url like `s3://mybucket/path/to/file.txt`
// and environment variables such as
// $ export AWS_ACCESS_KEY_ID=AKIAIOSFODNN7EXAMPLE
// $ export AWS_SECRET_ACCESS_KEY=wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY
// $ export AWS_DEFAULT_REGION=us-west-2
// $ export AWS_REGION=us-west-2

TEST(match_spec, parse_version_build)
{
    // S3Mirror::SignData s3mirror_signdata("GET", "");
    S3Mirror s3mirror_signdata("someurl");

    const auto p0 = std::chrono::time_point<std::chrono::system_clock>{};

    auto s = s3mirror_signdata.calculate_signature(
        p0,
        "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY",
        "eu",
        "s3",
        "thisisateststring");

    EXPECT_EQ(s, "5ab6621ee11b8e1fb42215c4f03f9b2e5808c608325da059b6df88dc2852ce93");

    auto s2 = s3mirror_signdata.calculate_signature(
        p0,
        "wXalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY",
        "eu",
        "s3",
        "thisisateststring");

    EXPECT_NE(s, s2);

    // s3mirror_signdata.add_extra_headers(nullptr);
}