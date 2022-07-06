#include <filesystem>
#include <spdlog/fmt/fmt.h>

#include <powerloader/mirrors/oci.hpp>

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

    std::string create_manifest(const OCILayer& config, const std::vector<OCILayer>& layers)
    {
        std::stringstream ss;
        nlohmann::json j;
        j["schemaVersion"] = 2;

        j["config"] = config.to_json();

        j["layers"] = nlohmann::json::array();

        for (auto& layer : layers)
        {
            j["layers"].push_back(layer.to_json());
        }

        return j.dump(4);
    }

    OCILayer OCILayer::from_file(const std::string& mime_type,
                                 const fs::path& file,
                                 const std::optional<nlohmann::json>& annotations)
    {
        return OCILayer(mime_type, file, std::nullopt, annotations);
    }

    OCILayer OCILayer::from_string(const std::string& mime_type,
                                   const std::string& content,
                                   const std::optional<nlohmann::json>& annotations)
    {
        return OCILayer(mime_type, std::nullopt, content, annotations);
    }

    OCILayer::OCILayer(const std::string& mime_type,
                       const std::optional<fs::path>& path,
                       const std::optional<std::string>& content,
                       const std::optional<nlohmann::json>& annotations)
        : mime_type(mime_type)
        , file(path)
        , contents(content)
        , annotations(annotations)
    {
        if (file)
        {
            digest = fmt::format("sha256:{}", sha256sum(file.value()));
            size = fs::file_size(file.value());
        }
        else
        {
            digest = fmt::format("sha256:{}", sha256(contents.value()));
            size = contents.value().size();
        }
    }


    Response OCILayer::upload(const Context& ctx,
                              const OCIMirror& mirror,
                              const std::string& reference) const
    {
        std::string preupload_url = mirror.get_preupload_url(reference);
        auto response = CURLHandle(ctx, preupload_url)
                            .setopt(CURLOPT_CUSTOMREQUEST, "POST")
                            .add_headers(mirror.get_auth_headers(reference))
                            .perform();

        if (!response.ok())
            return response;

        std::string temp_upload_location = response.headers.at("location");

        auto upload_url = format_upload_url(mirror.url, temp_upload_location, digest);

        spdlog::info("Uploading digest {}", digest);
        spdlog::info("Upload url: {}", upload_url);

        CURLHandle chandle(ctx, upload_url);
        // for uploading we always use application/octet-stream. The proper mimetypes
        // are defined in the manifest
        chandle.setopt(CURLOPT_UPLOAD, 1L)
            .add_headers(mirror.get_auth_headers(reference))
            .add_header(fmt::format("Content-Type: application/octet-stream", mime_type));

        if (file)
        {
            std::ifstream ufile(file.value(), std::ios::in | std::ios::binary);
            chandle.upload(ufile);
            return chandle.perform();
        }
        else
        {
            std::istringstream config_stream(contents.value());
            chandle.upload(config_stream);
            return chandle.perform();
        }
    }

    nlohmann::json OCILayer::to_json() const
    {
        auto json_layer = nlohmann::json::object();
        json_layer["mediaType"] = mime_type;
        json_layer["size"] = size;
        json_layer["digest"] = digest;
        if (annotations)
        {
            json_layer["annotations"] = annotations.value();
        }
        return json_layer;
    }

    Response oci_upload(const Context& ctx,
                        OCIMirror& mirror,
                        const std::string& reference,
                        const std::string& tag,
                        const std::vector<OCILayer>& layers,
                        const std::optional<OCILayer>& config)
    {
        // default is a empty json object
        OCILayer default_config
            = OCILayer::from_string("application/vnd.unknown.config.v1+json", std::string("{}"));
        OCILayer oci_layer_config = config.value_or(default_config);

        CURLHandle auth_handle{ ctx };
        if (mirror.need_auth() && mirror.prepare(reference, auth_handle))
        {
            auto auth_res = auth_handle.perform();
            if (!auth_res.ok())
            {
                spdlog::error("Could not authenticate to OCI Registry");
                return auth_res;
            }
        }

        for (auto& layer : layers)
        {
            auto upload_res = layer.upload(ctx, mirror, reference);

            if (!upload_res.ok())
                return upload_res;
        }

        // Upload the config, too
        {
            auto upload_res = oci_layer_config.upload(ctx, mirror, reference);
            if (!upload_res.ok())
                return upload_res;
        }

        // Now we need to upload the manifest for OCI servers
        std::string manifest_url = mirror.get_manifest_url(reference, tag);
        std::string manifest = create_manifest(oci_layer_config, layers);

        spdlog::info("Manifest: {}", manifest);
        std::istringstream manifest_stream(manifest);

        CURLHandle mhandle(ctx, manifest_url);
        mhandle.add_headers(mirror.get_auth_headers(reference))
            .add_header("Content-Type: application/vnd.oci.image.manifest.v1+json")
            .upload(manifest_stream);

        Response result = mhandle.perform();
        spdlog::info("Uploaded {} layers to {}:{}", layers.size(), mirror.get_repo(reference), tag);
        return result;
    }

}
