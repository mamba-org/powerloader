#include <filesystem>
#include <spdlog/fmt/fmt.h>

#include "mirrors/oci.hpp"

namespace powerloader
{
    std::string format_upload_url(const std::string& mirror_url,
                                  const std::string& temp_upload_location,
                                  const std::string& digest)
    {
        std::string upload_url;
        if (contains(temp_upload_location, "://"))
        {
            upload_url = fmt::format("{}", temp_upload_location);
        }
        else
        {
            upload_url = fmt::format("{}{}", mirror_url, temp_upload_location);
        }
        if (contains(upload_url, "?"))
        {
            upload_url = fmt::format("{}&digest={}", upload_url, digest);
        }
        else
        {
            upload_url = fmt::format("{}?digest={}", upload_url, digest);
        }
        return upload_url;
    }

    Response oci_upload(OCIMirror& mirror,
                        const std::string& reference,
                        const std::string& tag,
                        const fs::path& file)
    {
        std::string digest = fmt::format("sha256:{}", sha256sum(file));
        std::size_t fsize = fs::file_size(file);

        spdlog::info("Uploading {} with digest {}", file.string(), digest);

        CURLHandle auth_handle;
        if (mirror.need_auth() && mirror.prepare(reference, auth_handle))
        {
            auth_handle.perform();
        }

        std::string preupload_url = mirror.get_preupload_url(reference);
        auto response = CURLHandle(preupload_url)
                            .setopt(CURLOPT_CUSTOMREQUEST, "POST")
                            .add_headers(mirror.get_auth_headers(reference))
                            .perform();

        std::string temp_upload_location = response.header["location"];

        auto upload_url = format_upload_url(mirror.url, temp_upload_location, digest);
        spdlog::info("Upload url: {}", upload_url);

        CURLHandle chandle(upload_url);

        std::ifstream ufile(file, std::ios::in | std::ios::binary);
        chandle.setopt(CURLOPT_UPLOAD, 1L)
            .add_headers(mirror.get_auth_headers(reference))
            .add_header("Content-Type: application/octet-stream")
            .upload(ufile);
        auto upload_res = chandle.perform();


        // On certain registries, we also need to push the empty config
        if (!contains(upload_url, "ghcr.io"))
        {
            std::string preupload_url = mirror.get_preupload_url(reference);
            auto response = CURLHandle(preupload_url)
                                .setopt(CURLOPT_CUSTOMREQUEST, "POST")
                                .add_headers(mirror.get_auth_headers(reference))
                                .perform();

            std::string temp_upload_location = response.header["location"];

            // On certain registries, we also need to push the empty config
            upload_url = format_upload_url(
                mirror.url, temp_upload_location, fmt::format("sha256:{}", EMPTY_SHA));

            spdlog::info("Upload url: {}", upload_url);

            spdlog::info("Uploading empty config file");
            CURLHandle chandle_config(upload_url);
            std::istringstream emptyfile;
            chandle_config.setopt(CURLOPT_UPLOAD, 1L)
                .add_headers(mirror.get_auth_headers(reference))
                .add_header("Content-Type: application/vnd.unknown.config.v1+json")
                .upload(emptyfile);
            auto cres = chandle_config.perform();
        }

        // Now we need to upload the manifest for OCI servers
        std::string manifest_url = mirror.get_manifest_url(reference, tag);
        std::string manifest = mirror.create_manifest(fsize, digest);

        spdlog::info("Manifest: {}", manifest);
        std::istringstream manifest_stream(manifest);

        CURLHandle mhandle(manifest_url);
        mhandle.add_headers(mirror.get_auth_headers(reference))
            .add_header("Content-Type: application/vnd.oci.image.manifest.v1+json")
            .upload(manifest_stream);

        Response result = mhandle.perform();
        spdlog::info("Uploaded {} to {}:{}", file.string(), mirror.get_repo(reference), tag);
        return result;
    }

}
