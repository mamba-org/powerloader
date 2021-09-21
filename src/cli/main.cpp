#include <CLI/CLI.hpp>

#include "url.hpp"
#include "mirror.hpp"
#include "utils.hpp"
#include "mirrors/oci.hpp"
#include "mirrors/s3.hpp"

enum KindOf
{
    HTTP,
    OCI,
    S3
};

int handle_upload(const std::vector<std::string> &files, const std::vector<std::string> &mirrors)
{
    std::string mirror_url = mirrors[0];
    if (mirrors.size() > 1)
        std::cout << "Warning: only uploading to first mirror\n";

    KindOf kof = KindOf::HTTP;
    std::unique_ptr<Mirror> mptr;

    mamba::URLHandler url(mirror_url);

    if (url.scheme() == "s3")
        kof = KindOf::S3;
    else if (url.scheme() == "oci")
        kof = KindOf::OCI;

    if (kof != KindOf::HTTP)
        url.set_scheme("https");

    std::cout << "URL: " << url.url() << std::endl;

    for (auto &f : files)
    {
        auto elems = split(f, ":");
        fs::path fpath = elems[0];
        std::string dest = elems[1];

        if (kof == KindOf::OCI)
        {
            if (elems.size() != 3)
            {
                std::cout << "For OCI upload we need file:destname:tag" << std::endl;
                return 1;
            }
            std::string GH_SECRET = get_env("GHA_PAT");
            std::string GH_USER = get_env("GHA_USER");

            OCIMirror mirror(url.url(), "push", GH_USER, GH_SECRET);
            oci_upload(mirror, GH_USER + "/" + dest, elems[2], elems[0]);
        }
        else if (kof == KindOf::S3)
        {
            if (elems.size() != 2)
            {
                std::cout << "For S3 upload we need file:destpath" << std::endl;
                return 1;
            }

            std::string aws_ackey = get_env("AWS_ACCESS_KEY");
            std::string aws_sekey = get_env("AWS_SECRET_KEY");
            std::string aws_region = get_env("AWS_DEFAULT_REGION");

            std::string url_ = url.url();
            if (url_.back() == '/')
                url_ = url_.substr(0, url_.size() - 1);

            S3Mirror s3mirror(
                url_,
                aws_region,
                aws_ackey,
                aws_sekey);
            s3_upload(s3mirror, elems[1], elems[0]);
        }
    }

    return 1;
}

int main(int argc, char **argv)
{
    CLI::App app;

    std::vector<std::string> upload_files, download_files;
    std::vector<std::string> mirrors;

    CLI::App *s_dl = app.add_subcommand("download", "Download a file");
    s_dl->add_option("files", download_files, "Files to download");
    s_dl->add_option("-m", mirrors, "Mirror to upload to");

    CLI::App *s_ul = app.add_subcommand("upload", "Upload a file");
    s_ul->add_option("files", upload_files, "Files to upload");
    s_ul->add_option("-m", mirrors, "Mirror to upload to");

    CLI11_PARSE(app, argc, argv);

    if (app.got_subcommand("upload"))
    {
        return handle_upload(upload_files, mirrors);
    }
    if (app.got_subcommand("download"))
    {
        return 0;
    }

    return 0;
}
