#ifndef PL_OCI_HPP
#define PL_OCI_HPP

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

        using split_function_type
            = std::function<std::pair<std::string, std::string>(const std::string&)>;

        OCIMirror(const std::string& host, const std::string& repo_prefix);
        OCIMirror(const std::string& host,
                  const std::string& repo_prefix,
                  const std::string& scope,
                  const std::string& username,
                  const std::string& password);

        void set_fn_tag_split_function(const split_function_type& func);

        std::pair<std::string, std::string> split_path_tag(const std::string& path) const;

        std::string get_repo(const std::string& repo) const;
        std::string get_auth_url(const std::string& repo, const std::string& scope) const;
        std::string get_manifest_url(const std::string& repo, const std::string& reference) const;
        std::string get_preupload_url(const std::string& repo) const;

        AuthCallbackData* get_data(Target* target);
        std::vector<std::string> get_auth_headers(const std::string& path) override;

        // authenticate per target, and authentication state
        // is also dependent on each target unfortunately?!
        bool prepare(const std::string& path, CURLHandle& handle) override;
        bool need_auth() const;
        bool need_preparation(Target* target) override;

        // void add_extra_headers(Target* target);
        std::string format_url(Target* target) override;

        // upload specific functions
        std::string get_digest(const fs::path& p) const;
        std::string create_manifest(std::size_t size, const std::string& digest) const;

    private:
        std::map<std::string, std::unique_ptr<AuthCallbackData>> m_path_cb_map;
        std::string m_repo_prefix;
        std::string m_scope;
        std::string m_username;
        std::string m_password;
        split_function_type m_split_func;
    };

    Response oci_upload(OCIMirror& mirror,
                        const std::string& reference,
                        const std::string& tag,
                        const fs::path& file);

}

#endif
