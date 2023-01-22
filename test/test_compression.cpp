#ifdef WITH_ZSTD
#include <doctest/doctest.h>

#include "powerloader/downloader.hpp"
#include "powerloader/mirrors/oci.hpp"
#include "powerloader/mirrors/s3.hpp"

using namespace powerloader;

TEST_SUITE("mirror_id")
{
    TEST_CASE("create_mirror_id")
    {
        // test that this does not segfault
        std::string arg = "test";
        const auto new_id = Mirror::id(arg);
        CHECK_EQ(new_id.to_string(), "MirrorID <Mirror[test]>");

        std::string arg2 = "test2";
        const auto new_id2 = OCIMirror::id(arg, arg2);
        CHECK_EQ(new_id2.to_string(), "MirrorID <OCIMirror[test/test2]>");

        const auto new_id3 = S3Mirror::id(arg, arg2);
        CHECK_EQ(new_id3.to_string(), "MirrorID <S3Mirror[test/test2]>");
    }
}

TEST_SUITE("compression")
{
    TEST_CASE("download")
    {
        fs::path filename = fs::canonical("../testdata/f1.txt.zst");
        if (fs::exists("out_zst.txt"))
        {
            fs::remove("out_zst.txt");
        }

        Context ctx;
        std::string file_url = path_to_url(filename.string());
        auto target = DownloadTarget::from_url(ctx, file_url, "out_zst.txt", "");

        target->set_compression_type(CompressionType::ZSTD);
        target->add_checksum(
            { ChecksumType::kSHA256,
              "06fa557926742aad170074b1ce955014a4213e960e8f09f07fa23371100dd18e" });

        Downloader downloader(ctx);
        downloader.add(target);
        downloader.download();

        CHECK_FALSE(target->get_error().has_value());
        CHECK(fs::exists("out_zst.txt"));

        std::ifstream ifs("out_zst.txt");
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            (std::istreambuf_iterator<char>()));
        std::string beginning = "Lorem ipsum dolor sit amet, consetetur";
        CHECK_EQ(content.substr(0, beginning.size()), beginning);

        std::ifstream orig_fs("../testdata/f1.txt");

        std::string orig_content((std::istreambuf_iterator<char>(orig_fs)),
                                 (std::istreambuf_iterator<char>()));

        CHECK_EQ(content, orig_content);
    }
}

#endif
