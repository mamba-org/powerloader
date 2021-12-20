#pragma once

#include <spdlog/fmt/fmt.h>
#include "mirror.hpp"

namespace powerloader
{
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
        std::string repo_prefix, scope, username, password;
        std::function<std::pair<std::string, std::string>(const std::string&)> m_split_func;

        OCIMirror(const std::string& host, const std::string& repo_prefix)
            : Mirror(host)
            , repo_prefix(repo_prefix)
            , scope("pull")
        {
        }

        OCIMirror(const std::string& host,
                  const std::string& repo_prefix,
                  const std::string& scope,
                  const std::string& username,
                  const std::string& password)
            : Mirror(host)
            , repo_prefix(repo_prefix)
            , scope(scope)
            , username(username)
            , password(password)
        {
        }

        void set_fn_tag_split_function(
            const std::function<std::pair<std::string, std::string>(const std::string&)>& func)
        {
            m_split_func = func;
        }

        std::pair<std::string, std::string> split_path_tag(const std::string& path) const;

        std::string get_repo(const std::string& repo)
        {
            if (!repo_prefix.empty())
                return fmt::format("{}/{}", repo_prefix, repo);
            else
                return repo;
        }
        std::string get_auth_url(const std::string& repo, const std::string& scope)
        {
            return fmt::format("{}/token?scope=repository:{}:{}", url, get_repo(repo), scope);
        }

        std::string get_manifest_url(const std::string& repo, const std::string& reference)
        {
            return fmt::format("{}/v2/{}/manifests/{}", url, get_repo(repo), reference);
        }

        std::string get_preupload_url(const std::string& repo)
        {
            return fmt::format("{}/v2/{}/blobs/uploads/", url, get_repo(repo));
        }

        AuthCallbackData* get_data(Target* target);

        std::vector<std::string> get_auth_headers(const std::string& path);

        // authenticate per target, and authentication state
        // is also dependent on each target unfortunately?!
        bool prepare(const std::string& path, CURLHandle& handle);
        bool need_auth() const;
        virtual bool need_preparation(Target* target);

        // void add_extra_headers(Target* target);
        std::string format_url(Target* target);

        // upload specific functions
        std::string get_digest(const fs::path& p);
        std::string create_manifest(std::size_t size, const std::string& digest);
    };

    Response oci_upload(OCIMirror& mirror,
                        const std::string& reference,
                        const std::string& tag,
                        const fs::path& file);

}
