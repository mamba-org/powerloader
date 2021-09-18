#pragma once

#include "mirror.hpp"

struct OCIMirror : public Mirror
{
    struct AuthCallbackData
    {
        OCIMirror *self;
        Target *target;
        std::string sha256sum, token, buffer;
    };

    std::unique_ptr<AuthCallbackData> auth_callback_data;

    std::map<std::string, std::unique_ptr<AuthCallbackData>> path_cb_map;

    OCIMirror(const std::string &url)
        : Mirror(url)
    {
    }

    std::string get_auth_url(const std::string &repo, const std::string &scope)
    {
        return fmt::format("{}/token?scope=repository:{}:{}", mirror.url, repo, scope);
    }

    std::string get_manifest_url(const std::string &repo, const std::string &reference)
    {
        return fmt::format("{}/v2/{}/manifests/{}", mirror.url, repo, reference);
    }

    AuthCallbackData* get_data(Target* target);

    static std::size_t auth_write_callback(char *buffer, std::size_t size, std::size_t nitems, AuthCallbackData *self);
    
    static CbReturnCode finalize_auth_callback(TransferStatus status, const std::string &msg, void *data);
    static CbReturnCode finalize_manifest_callback(TransferStatus status, const std::string &msg, void *data);

    bool prepare(const std::string& path, CURLHandle& handle);
    virtual bool need_preparation(Target *target);

    // authenticate per target, and authentication state
    // is also dependent on each target unfortunately?!
    virtual bool prepare(Target *target);
    void add_extra_headers(Target* target);
    std::string format_url(Target *target);

    virtual bool authenticate(CURLHandle& handle, const std::string& path);


    // upload specific functions
    std::string get_digest(const fs::path& p);
    std::string create_manifest(std::size_t size, const std::string& digest);
};