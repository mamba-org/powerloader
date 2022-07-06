#include <spdlog/fmt/fmt.h>

#include <powerloader/mirrors/oci.hpp>
#include <powerloader/target.hpp>

namespace powerloader
{
    // OCI Mirror:
    // When knowing the SHA256 we can directly get to the blob
    // When we do not know the SHA256 sum, we need to find the `latest` or some
    // other blob

    // OCI upload process
    // 4 steps:
    //  - first get auth token with push rights
    //  - then

    // This is what an OCI manifest (index) looks like:
    // {
    //     "schemaVersion": 2,
    //     "config": {
    //         "mediaType": "application/vnd.unknown.config.v1+json",
    //         "digest":
    //         "sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    //         "size": 0
    //     },
    //     "layers": [
    //         {
    //             "mediaType": "application/vnd.unknown.layer.v1+txt",
    //             "digest":
    //             "sha256:c5be3ea75353851e1fcf3a298af3b6cfd2af3d7ff018ce52657b6dbd8f986aa4",
    //             "size": 13,
    //             "annotations": {
    //                 "org.opencontainers.image.title": "artifact.txt"
    //             }
    //         }
    //     ]
    // }

    OCIMirror::OCIMirror(const Context& ctx,
                         const std::string& host,
                         const std::string& repo_prefix)
        : Mirror(ctx, host)
        , m_repo_prefix(repo_prefix)
        , m_scope("pull")
    {
    }

    OCIMirror::OCIMirror(const Context& ctx,
                         const std::string& host,
                         const std::string& repo_prefix,
                         const std::string& scope,
                         const std::string& username,
                         const std::string& password)
        : Mirror(ctx, host)
        , m_repo_prefix(repo_prefix)
        , m_scope(scope)
        , m_username(username)
        , m_password(password)
    {
    }

    void OCIMirror::set_fn_tag_split_function(const split_function_type& func)
    {
        m_split_func = func;
    }

    std::pair<std::string, std::string> OCIMirror::split_path_tag(const std::string& path) const
    {
        std::string split_path, split_tag;
        if (m_split_func)
        {
            std::tie(split_path, split_tag) = m_split_func(path);
        }
        else
        {
            split_path = path;
            split_tag = "latest";
        }
        return std::make_pair(split_path, split_tag);
    }

    std::string OCIMirror::get_repo(const std::string& repo) const
    {
        if (!m_repo_prefix.empty())
            return fmt::format("{}/{}", m_repo_prefix, repo);
        else
            return repo;
    }
    std::string OCIMirror::get_auth_url(const std::string& repo, const std::string& scope) const
    {
        return fmt::format("{}/token?scope=repository:{}:{}", url, get_repo(repo), scope);
    }

    std::string OCIMirror::get_manifest_url(const std::string& repo,
                                            const std::string& reference) const
    {
        return fmt::format("{}/v2/{}/manifests/{}", url, get_repo(repo), reference);
    }

    std::string OCIMirror::get_preupload_url(const std::string& repo) const
    {
        return fmt::format("{}/v2/{}/blobs/uploads/", url, get_repo(repo));
    }

    OCIMirror::AuthCallbackData* OCIMirror::get_data(Target* target)
    {
        auto [split_path, _] = split_path_tag(target->target->path);
        auto it = m_path_cb_map.find(split_path);
        if (it != m_path_cb_map.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    std::vector<std::string> OCIMirror::get_auth_headers(const std::string& path) const
    {
        if (m_username.empty() && m_password.empty())
            return {};
        auto [split_path, _] = split_path_tag(path);
        auto& data = m_path_cb_map.at(split_path);
        return { fmt::format("Authorization: Bearer {}", data->token) };
    }

    bool OCIMirror::prepare(const std::string& path, CURLHandle& handle)
    {
        auto [split_path, split_tag] = split_path_tag(path);

        auto it = m_path_cb_map.find(split_path);
        if (it == m_path_cb_map.end())
        {
            m_path_cb_map[split_path].reset(new AuthCallbackData);
            auto data = m_path_cb_map[split_path].get();
            data->self = this;
        }

        auto& cbdata = m_path_cb_map[split_path];

        if (cbdata->token.empty() && need_auth())
        {
            std::string auth_url = get_auth_url(split_path, m_scope);
            handle.url(auth_url);

            handle.set_default_callbacks();

            if (!m_username.empty())
            {
                handle.setopt(CURLOPT_USERNAME, m_username.c_str());
            }
            if (!m_password.empty())
            {
                handle.setopt(CURLOPT_PASSWORD, m_password.c_str());
            }

            auto end_callback = [this, &cbdata](const Response& response)
            {
                if (!response.ok())
                    return CbReturnCode::kERROR;

                auto j = response.json();
                if (j.contains("token"))
                {
                    cbdata->token = j["token"].get<std::string>();
                    return CbReturnCode::kOK;
                }
                return CbReturnCode::kERROR;
            };

            handle.set_end_callback(end_callback);
        }
        else
        {
            handle.set_default_callbacks();

            std::string manifest_url = get_manifest_url(split_path, split_tag);

            handle.url(manifest_url)
                .add_headers(get_auth_headers(path))
                .add_header("Accept: application/vnd.oci.image.manifest.v1+json");

            auto finalize_manifest_callback = [this, &cbdata](const Response& response)
            {
                if (!response.ok())
                    return CbReturnCode::kERROR;
                auto j = response.json();

                std::cout << "Got manifest callback!" << std::endl;
                std::cout << j.dump(4) << std::endl;

                if (j.contains("layers"))
                {
                    std::string digest = j["layers"][0]["digest"];
                    std::size_t expected_size = j["layers"][0]["size"];

                    assert(starts_with(digest, "sha256:"));

                    // For some reason target->target isn't available here?
                    // cbdata->target->target->checksums.push_back(
                    //     Checksum{ChecksumType::kSHA256, digest.substr(sizeof("sha256:") - 1)}
                    // );

                    cbdata->sha256sum = digest.substr(sizeof("sha256:") - 1);
                    return CbReturnCode::kOK;
                }

                return CbReturnCode::kERROR;
            };

            handle.set_end_callback(finalize_manifest_callback);
        }
        return true;
    }

    bool OCIMirror::need_auth() const
    {
        return m_username.size() && m_password.size();
    }

    bool OCIMirror::need_preparation(Target* target)
    {
        auto* data = get_data(target);
        if ((!data || data && data->token.empty()) && need_auth())
            return true;

        if (data && !data->sha256sum.empty())
            return false;

        if (std::none_of(target->target->checksums.begin(),
                         target->target->checksums.end(),
                         [](auto& ck) { return ck.type == ChecksumType::kSHA256; }))
            return true;

        return false;
    }

    std::string OCIMirror::format_url(Target* target)
    {
        std::string* checksum = nullptr;

        for (auto& ck : target->target->checksums)
        {
            if (ck.type == ChecksumType::kSHA256)
                checksum = &ck.checksum;
        }

        if (!checksum)
        {
            auto* data = get_data(target);
            checksum = &data->sha256sum;
        }
        auto [split_path, split_tag] = split_path_tag(target->target->path);
        // https://ghcr.io/v2/wolfv/artifact/blobs/sha256:c5be3ea75353851e1fcf3a298af3b6cfd2af3d7ff018ce52657b6dbd8f986aa4
        return fmt::format("{}/v2/{}/blobs/sha256:{}", url, get_repo(split_path), *checksum);
    }

    std::string OCIMirror::get_digest(const fs::path& p) const
    {
        return fmt::format("sha256:{}", sha256sum(p));
    }
}
