#include <filesystem>
#include <spdlog/fmt/fmt.h>

#include "mirrors/oci.hpp"

namespace powerloader
{
    Response oci_upload(OCIMirror& mirror,
                        const std::string& reference,
                        const std::string& tag,
                        const fs::path& file)
    {
        std::string digest = fmt::format("sha256:{}", sha256sum(file));
        std::size_t fsize = fs::file_size(file);

        CURLHandle auth_handle;
        if (mirror.prepare(reference, auth_handle))
        {
            auth_handle.perform();
        }

        std::string preupload_url = mirror.get_preupload_url(reference);
        auto response = CURLHandle(preupload_url)
                            .setopt(CURLOPT_CUSTOMREQUEST, "POST")
                            .add_headers(mirror.get_auth_headers(reference))
                            .perform();

        std::string temp_upload_location = response.header["location"];
        std::string upload_url
            = fmt::format("{}{}?digest={}", mirror.url, temp_upload_location, digest);

        spdlog::info("Upload url: {}", upload_url);

        CURLHandle chandle(upload_url);

        std::ifstream ufile(file, std::ios::in | std::ios::binary);
        chandle.setopt(CURLOPT_UPLOAD, 1L)
            .add_headers(mirror.get_auth_headers(reference))
            .add_header("Content-Type: application/octet-stream")
            .upload(ufile);

        auto cres = chandle.perform();

        // Now we need to upload the manifest for OCI servers
        std::string manifest_url = fmt::format("{}/v2/{}/manifests/{}", mirror.url, reference, tag);
        std::string manifest = mirror.create_manifest(fsize, digest);

        std::istringstream manifest_stream(manifest);

        CURLHandle mhandle(manifest_url);
        mhandle.add_headers(mirror.get_auth_headers(reference))
            .add_header("Content-Type: application/vnd.oci.image.manifest.v1+json")
            .upload(manifest_stream);

        return mhandle.perform();
    }

}
