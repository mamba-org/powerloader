#pragma once

#include "mirror.hpp"

struct OCIMirror : public Mirror
{
    struct AuthCallbackData
    {
        OCIMirror* self;
        Target* target;
        Response response;
        std::string sha256sum, token, buffer;
    };

    std::map<std::string, std::unique_ptr<AuthCallbackData>> path_cb_map;
    std::string scope, username, password;

    OCIMirror(const std::string& url)
        : Mirror(url)
        , scope("pull")
    {
    }

    OCIMirror(const std::string& url,
              const std::string& scope,
              const std::string& username,
              const std::string& password)
        : Mirror(url)
        , scope(scope)
        , username(username)
        , password(password)
    {
    }

    std::string get_auth_url(const std::string& repo, const std::string& scope)
    {
        return fmt::format("{}/token?scope=repository:{}:{}", url, repo, scope);
    }

    std::string get_manifest_url(const std::string& repo, const std::string& reference)
    {
        return fmt::format("{}/v2/{}/manifests/{}", url, repo, reference);
    }

    std::string get_preupload_url(const std::string& repo)
    {
        return fmt::format("{}/v2/{}/blobs/uploads/", url, repo);
    }

    AuthCallbackData* get_data(Target* target);

    std::vector<std::string> get_auth_headers(const std::string& path);

    // authenticate per target, and authentication state
    // is also dependent on each target unfortunately?!
    bool prepare(const std::string& path, CURLHandle& handle);
    virtual bool need_preparation(Target* target);

    // void add_extra_headers(Target* target);
    std::string format_url(Target* target);

    // upload specific functions
    std::string get_digest(const fs::path& p);
    std::string create_manifest(std::size_t size, const std::string& digest);
};

Response
oci_upload(OCIMirror& mirror,
           const std::string& reference,
           const std::string& tag,
           const fs::path& file);
