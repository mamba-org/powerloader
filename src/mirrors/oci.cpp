#include "target.hpp"
#include "mirrors/oci.hpp"

// OCI Mirror:
// When knowing the SHA256 we can directly get to the blob
// When we do not know the SHA256 sum, we need to find the `latest` or some other blob

bool OCIMirror::need_preparation(Target* target)
{
    auto *data = get_data(target);
    if (data && data->token.empty())
        return true;
    
    if (std::none_of(target->target->checksums.begin(), target->target->checksums.end(), [](auto &ck)
                    { return ck.type == ChecksumType::SHA256; }))
        return true;
    return false;
}

OCIMirror::AuthCallbackData *OCIMirror::get_data(Target *target)
{
    auto it = path_cb_map.find(target->target->path);
    if (it != path_cb_map.end())
    {
        return path_cb_map[target->target->path].get();
    }
    return nullptr;
}

CbReturnCode OCIMirror::finalize_manifest_callback(TransferStatus status, const std::string &msg, void *data)
{
    assert(data);
    AuthCallbackData *d = static_cast<AuthCallbackData *>(data);

    d->target->override_endcb = nullptr;
    auto j = nlohmann::json::parse(d->buffer);

    // This is what an OCI manifest (index) looks like:
    // {
    //     "schemaVersion": 2,
    //     "config": {
    //         "mediaType": "application/vnd.unknown.config.v1+json",
    //         "digest": "sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    //         "size": 0
    //     },
    //     "layers": [
    //         {
    //             "mediaType": "application/vnd.unknown.layer.v1+txt",
    //             "digest": "sha256:c5be3ea75353851e1fcf3a298af3b6cfd2af3d7ff018ce52657b6dbd8f986aa4",
    //             "size": 13,
    //             "annotations": {
    //                 "org.opencontainers.image.title": "artifact.txt"
    //             }
    //         }
    //     ]
    // }

    if (j.contains("layers"))
    {
        std::string digest = j["layers"][0]["digest"];
        std::size_t expected_size = j["layers"][0]["size"];
        assert(starts_with(digest, "sha256:"));

        // TODO check if we should push the checksums into the download target or do something
        //      on the mirror level?!
        d->target->target->checksums.push_back(Checksum{.type = ChecksumType::SHA256, .checksum = digest.substr(sizeof("sha256:") - 1)});
        // path_shasum[d->target->target->path] = digest.substr(sizeof("sha256:"));
        CbReturnCode::OK;
    }
    return CbReturnCode::ERROR;
}

std::size_t OCIMirror::auth_write_callback(char *buffer, std::size_t size, std::size_t nitems, AuthCallbackData *self)
{
    std::string received_chunk(buffer, size * nitems);
    self->buffer += received_chunk;
    return size * nitems;
}

bool OCIMirror::prepare(const std::string& path, CURLHandle& handle)
{
    auto it = path_cb_map.find(path);
    if (it == path_cb_map.end())
    {
        path_cb_map[path].reset(new AuthCallbackData);
        auto data = path_cb_map[path].get();
        data->self = this;
    }

    auto& cbdata = path_cb_map[path];

    if (cbdata->token.empty())
    {
        std::string auth_url = get_auth_url(path, "pull");
        handle.url(auth_url);

        auto end_callback = [this, &cbdata](const Response& response) {
            auto j = response.json();
            std::cout << j.dump(4) << std::endl;
            if (j.contains("token"))
            {
                std::cout << "XValue " << j["token"] << std::endl;
                cbdata->token = j["token"].get<std::string>();
                return 0;
                // return CbReturnCode::OK;
            }
            return 1;
            // return CbReturnCode::ERROR;
        };

        handle.set_end_callback(end_callback);
    }

    return true;

}

bool OCIMirror::prepare(Target *target)
{
    AuthCallbackData* data = get_data(target);

    if (!data || data->token.empty())
    {
        std::string auth_url = get_auth_url(target->target->path, "pull");
        std::cout << "Fetching dl from " << auth_url << std::endl;
        target->setopt(CURLOPT_URL, auth_url.c_str());

        if (!data)
        {
            path_cb_map[target->target->path].reset(new AuthCallbackData);
            data = path_cb_map[target->target->path].get();
            data->self = this;
            data->target = target;
        }

        target->setopt(CURLOPT_WRITEFUNCTION, OCIMirror::auth_write_callback);
        target->setopt(CURLOPT_WRITEDATA, data);

        target->override_endcb = OCIMirror::finalize_auth_callback;
        target->override_endcb_data = (void *)data;
    }
    else
    {
        data->buffer.clear();
        std::string manifest_url = get_manifest_url(target->target->path, "1.0");
        std::cout << "Fetching manifest from " << manifest_url << std::endl;
        target->setopt(CURLOPT_URL, manifest_url.c_str());

        target->setopt(CURLOPT_WRITEFUNCTION, OCIMirror::auth_write_callback);
        target->setopt(CURLOPT_WRITEDATA, data);

        add_extra_headers(target);

        target->add_header("Accept: application/vnd.oci.image.manifest.v1+json");
        target->override_endcb = OCIMirror::finalize_manifest_callback;
        target->override_endcb_data = (void *)data;
    }

    return true;
}

std::string OCIMirror::format_url(Target *target)
{
    std::string* checksum = nullptr;

    for (auto& ck : target->target->checksums)
    {
        if (ck.type == ChecksumType::SHA256)
            checksum = &ck.checksum;
    }
    
    if (!checksum)
    {
        auto* data = get_data(target);
        checksum = &data->sha256sum;
    }

    // https://ghcr.io/v2/wolfv/artifact/blobs/sha256:c5be3ea75353851e1fcf3a298af3b6cfd2af3d7ff018ce52657b6dbd8f986aa4
    return fmt::format("{}/v2/{}/blobs/sha256:{}",
                       mirror.url,
                       target->target->path,
                       *checksum);
}

CbReturnCode OCIMirror::finalize_auth_callback(TransferStatus status, const std::string &msg, void *data)
{
    assert(data);
    AuthCallbackData *d = static_cast<AuthCallbackData *>(data);

    d->target->override_endcb = nullptr;

    std::cout << "Transfer finalized, got: " << d->buffer << std::endl;
    auto j = nlohmann::json::parse(d->buffer);
    if (j.contains("token"))
    {
        std::cout << "Value " << j["token"] << std::endl;
        d->token = j["token"].get<std::string>();
        return CbReturnCode::OK;
    }
    return CbReturnCode::ERROR;
}

void OCIMirror::add_extra_headers(Target *target)
{
    auto* data = get_data(target);
    target->add_header(fmt::format("Authorization: Bearer {}", data->token).c_str());
}

bool OCIMirror::authenticate(CURLHandle& handle,
                             const std::string& path)
{
    auto it = path_cb_map.find(path);
    if (it == path_cb_map.end())
    {
        return false;
    }

    handle.add_header(fmt::format("Authorization: Bearer {}", path_cb_map[path]->token));
    return true;
}

std::string OCIMirror::create_manifest(std::size_t size, const std::string& digest)
{
    std::stringstream ss;
    nlohmann::json j;
    j["schemaVersion"] = 2;

    auto config = nlohmann::json::object();
    config["mediaType"] = "application/vnd.unknown.config.v1+json";
    // this is the sha256 of an empty string!
    config["digest"] = "sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    config["size"] = 0;
    j["config"] = config;

    j["layers"] = nlohmann::json::array();

    auto layer = nlohmann::json::object();
    layer["mediaType"] = "application/octet-stream";
    layer["size"] = size;
    layer["digest"] = digest;

    j["layers"].push_back(layer);
    ss << j.dump(4);
    return ss.str();
}

std::string OCIMirror::get_digest(const fs::path& p)
{
    return fmt::format("sha256:{}", sha256sum(p));
}


// OCI upload process
// 4 steps:
//  - first get auth token with push rights
//  - then 