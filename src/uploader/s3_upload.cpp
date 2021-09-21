#include "mirrors/s3.hpp"
#include <filesystem>

void
s3_upload(S3Mirror& mirror, const std::string& path, const fs::path& file)
{
    std::string digest = sha256sum(file);
    std::size_t fsize = fs::file_size(file);

    S3CanonicalRequest request("PUT", mirror.mirror.url, path, digest);
    request.hashed_payload = digest;

    // this is enough to make a file completely public
    // request.headers["x-amz-acl"] = "public-read";

    CURLHandle uploadrequest(fmt::format("{}/{}", mirror.mirror.url, path));

    uploadrequest.setopt(CURLOPT_UPLOAD, 1L)
        .setopt(CURLOPT_VERBOSE, 1L)
        .setopt(CURLOPT_INFILESIZE_LARGE, fsize)
        .add_headers(mirror.get_auth_headers(request));

    std::ifstream ufile(file, std::ios::in | std::ios::binary);
    set_file_upload_callback(uploadrequest, &ufile);

    auto ret = uploadrequest.perform();

    std::cout << ret.content.str() << std::endl;
}
