#include <filesystem>
#include "mirrors/s3.hpp"

void s3_upload(S3Mirror& mirror, 
               const std::string& path,
               const fs::path& file)
{
    std::string digest = sha256sum(file);
    std::size_t fsize = fs::file_size(file);


    // PUT /my-image.jpg HTTP/1.1
    // Host: myBucket.s3.<Region>.amazonaws.com
    // Date: Wed, 12 Oct 2009 17:50:00 GMT
    // Authorization: authorization string
    // Content-Type: text/plain
    // Content-Length: 11434
    // x-amz-meta-author: Janet
    // Expect: 100-continue

    S3CanonicalRequest request("PUT",
                       "https://wolfsuperbuckettest.s3.amazonaws.com",
                       path,
                       digest);
    request.hashed_payload = digest;

    CURLHandle uploadrequest(fmt::format("https://wolfsuperbuckettest.s3.amazonaws.com/{}", path));
    uploadrequest
        // .setopt(CURLOPT_UPLOAD, 1L)
        .setopt(CURLOPT_PUT, 1L)
        .setopt(CURLOPT_VERBOSE, 1L)
        .setopt(CURLOPT_INFILESIZE_LARGE, fsize)
        .add_headers(mirror.get_auth_headers(request))
        // .add_header(fmt::format("Content-Length: {}", fsize))
        // .add_header("Content-Type: application/octet-stream")
    ;

    std::ifstream ufile(file, std::ios::in | std::ios::binary);
    set_file_upload_callback(uploadrequest, &ufile);

    auto ret = uploadrequest.perform();
    std::cout << ret.content.str() << std::endl;
}