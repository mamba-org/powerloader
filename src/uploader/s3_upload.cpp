#include <filesystem>
#include <spdlog/fmt/fmt.h>

#include <powerloader/mirrors/s3.hpp>
#include "curl_internal.hpp"

namespace powerloader
{
    Response s3_upload(const Context& ctx,
                       S3Mirror& mirror,
                       const std::string& path,
                       const fs::path& file)
    {
        std::string digest = sha256sum(file);
        std::size_t fsize = fs::file_size(file);

        URLHandler uh(fmt::format("{}/{}", mirror.url(), path));

        S3CanonicalRequest request("PUT", uh, digest);
        request.hashed_payload = digest;

        // this is enough to make a file completely public
        // request.headers["x-amz-acl"] = "public-read";

        CURLHandle uploadrequest(ctx, uh.url());

        std::ifstream ufile(file, std::ios::in | std::ios::binary);

        CURLInterface::set_opt_wrapped(uploadrequest, CURLOPT_INFILESIZE_LARGE, fsize)
            .add_headers(mirror.get_auth_headers(request))
            .upload(ufile);

        return uploadrequest.perform();
    }
}
