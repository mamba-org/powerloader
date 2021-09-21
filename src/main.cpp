#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "downloader.hpp"
#include "fastest_mirror.hpp"
#include "zck.hpp"

#include "mirrors/oci.hpp"
#include "mirrors/s3.hpp"

class Channel
{
public:
    std::vector<std::string> mirrors;
};

int
main()
{
    std::cout << "Lets go download some files." << std::endl;
    zck_set_log_level(ZCK_LOG_DEBUG);

    // fastest_mirror({
    //     "https://github.com",
    //     "https://gitter.im",
    //     "https://conda.anaconda.org/conda-forge/linux-64/repodata.json",
    //     "https://ghcr.io/",
    //     "https://reddit.com",
    //     "https://facebook.com",
    //     "https://zeit.de",
    //     "http://mirrors.eze.sysarmy.com/ubuntu/"
    // });

    DownloadTarget dlauth("wolfv/artifact", "conda-forge", "artifact.txt");
    Target t(&dlauth);
    // t.perform();

    // DownloadTarget zcktest(
    //     "f1.txt.zck",
    //     "http://localhost:8000",
    //     "f1.txt");

    // // zcktest.checksums = {Checksum{.type = ChecksumType::SHA256, .checksum =
    // (std::string("06fa557926742aad170074b1ce955014a4213e960e8f09f07fa23371100dd18e"))}};
    // zcktest.checksums = {
    //     Checksum{.type = ChecksumType::SHA256, .checksum =
    //     (std::string("1dd70b69d2542b5d5287aa7c2064b76d3c3c3dc49bc698514d89b5ad11ab883f"))}};
    // zcktest.zck_header_size = 257;
    // Target t(&zcktest);

    // Mirror m1("https://github.com");
    std::string aws_ackey = get_env("AWS_ACCESS_KEY");
    std::string aws_sekey = get_env("AWS_SECRET_KEY");
    S3Mirror s3mirror(
        "https://wolfsuperbuckettest.s3.amazonaws.com", "eu-central-1", aws_ackey, aws_sekey);

    DownloadTarget s3target("test/test.yml", "s3mirror", "justfortest.yaml");

    std::vector<Mirror*> s3ms({ &s3mirror });

    OCIMirror m1("https://ghcr.io");
    Mirror m2("https://conda.anaconda.org/conda-forge");
    // Mirror m3("https://beta.mamba.pm/conda-forge");

    // std::vector<Mirror*> ms({&m1, &m2, &m3});
    std::vector<Mirror*> ms({ &m1 });

    DownloadTarget a("https://github.com", "", "gh.html");

    DownloadTarget b("https://quantstack.net", "", "qs.html");

    DownloadTarget c(
        "linux-64/xtensor-0.23.10-h4bd325d_0.tar.bz2", "conda-forge", "xtensor.tar.bz2");

    // c.resume = true;
    // c.expected_size = 12345;
    // c.byterange_start = 0;
    // c.byterange_end = 10000;

    std::map<std::string, std::vector<Mirror*>> mirror_map;
    mirror_map["conda-forge"] = ms;
    mirror_map["s3mirror"] = s3ms;

    Downloader dl;
    dl.mirror_map = mirror_map;

    // dl.add(&s3target);
    dl.add(&dlauth);

    // dl.add(&dlauth);
    // dl.add(&zcktest);
    // dl.add(&a);
    // dl.add(&b);
    // dl.add(&c);
    dl.download();
}
