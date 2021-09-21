#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <stdio.h>
#include <sys/stat.h>

#include "mirrors/oci.hpp"
#include "utils.hpp"

#include "./uploader/oci_upload.cpp"
#include "./uploader/s3_upload.cpp"

// static size_t manifest_upload(char *ptr, size_t size, size_t nmemb,
// ManifestReadData *userdata)
// {
//     if (userdata->read_pos == 0)
//         return 0;

//     std::size_t to_read = (size * nmemb) > userdata->manifest.size() ?
//     userdata->manifest.size() : size * nmemb;

//     userdata->read_pos -= to_read;
//     memcpy(ptr, userdata->manifest.c_str(), to_read);
//     fprintf(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T " bytes from
//     file\n", to_read); return to_read;
// }

void
add_multipart_upload(CURL* target,
                     const std::vector<std::string>& files,
                     const std::map<std::string, std::string>& extra_fields)
{
    curl_mimepart* part;
    curl_mime* mime;
    mime = curl_mime_init(target);

    for (const auto& f : files)
    {
        part = curl_mime_addpart(mime);
        curl_mime_filedata(part, f.c_str());
        curl_mime_name(part, "files");
    }

    for (const auto& [k, v] : extra_fields)
    {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, k.c_str());
        curl_mime_data(part, v.c_str(), CURL_ZERO_TERMINATED);
    }

    curl_easy_setopt(target, CURLOPT_MIMEPOST, mime);
}

int
main(void)
{
    std::string GH_SECRET = getenv("GHA_PAT");
    std::string GH_USER = "wolfv";

    OCIMirror mirror("https://ghcr.io", "push", GH_USER, GH_SECRET);
    oci_upload(mirror, "wolfv/xtensor", "1.1", "xtensor-0.23.10-hc021e02_0.tar.bz2");

    std::string aws_ackey = get_env("AWS_ACCESS_KEY");
    std::string aws_sekey = get_env("AWS_SECRET_KEY");
    std::string aws_region = get_env("AWS_DEFAULT_REGION");
    S3Mirror s3mirror("https://wolfsuperbuckettest.s3.eu-central-1.amazonaws.com",
                      aws_region,
                      aws_ackey,
                      aws_sekey);
    s3_upload(s3mirror, "xtensor-file.tar.bz2", "xtensor-0.23.10-hc021e02_0.tar.bz2");

    exit(0);
}
