#ifdef WITH_ZSTD
#include <gtest/gtest.h>

#include "powerloader/downloader.hpp"

using namespace powerloader;

TEST(compression, download)
{
    fs::path filename = fs::canonical("../testdata/f1.txt.zst");
    if (fs::exists("out_zst.txt"))
    {
        fs::remove("out_zst.txt");
    }
    auto target = std::make_shared<DownloadTarget>(
        "file:///" + filename.string(), "test.txt", "out_zst.txt");
    target->set_compression_type(CompressionType::ZSTD);
    target->add_checksum({ ChecksumType::kSHA256,
                           "06fa557926742aad170074b1ce955014a4213e960e8f09f07fa23371100dd18e" });

    Context ctx;
    Downloader downloader(ctx);
    downloader.add(target);
    downloader.download();

    EXPECT_EQ(target->get_error().has_value(), false);
    EXPECT_TRUE(fs::exists("out_zst.txt"));

    std::ifstream ifs("out_zst.txt");
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    std::string beginning = "Lorem ipsum dolor sit amet, consetetur";
    EXPECT_EQ(content.substr(0, beginning.size()), beginning);

    std::ifstream orig_fs("../testdata/f1.txt");

    std::string orig_content((std::istreambuf_iterator<char>(orig_fs)),
                             (std::istreambuf_iterator<char>()));

    EXPECT_EQ(content, orig_content);
}
#endif
