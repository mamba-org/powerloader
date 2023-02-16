#ifndef POWERLOADER_OCI_HPP
#define POWERLOADER_OCI_HPP

#include <optional>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <powerloader/export.hpp>
#include <powerloader/mirror.hpp>

namespace powerloader
{
    class Response;

    class POWERLOADER_API OCIMirror : public Mirror
    {
    public:
        using split_function_type
            = std::function<std::pair<std::string, std::string>(const std::string&)>;

        OCIMirror(const Context& ctx, const std::string& host, const std::string& repo_prefix);
        OCIMirror(const Context& ctx,
                  const std::string& host,
                  const std::string& repo_prefix,
                  const std::string& scope,
                  const std::string& username,
                  const std::string& password);
        ~OCIMirror();


        std::string get_repo(const std::string& repo) const;
        std::string get_auth_url(const std::string& repo, const std::string& scope) const;
        std::string get_manifest_url(const std::string& repo, const std::string& reference) const;
        std::string get_preupload_url(const std::string& repo) const;

        std::vector<std::string> get_auth_headers(const std::string& path) const override;

        // authenticate per target, and authentication state
        // is also dependent on each target unfortunately?!
        bool prepare(const std::string& path, CURLHandle& handle) override;
        bool need_auth() const;
        bool needs_preparation(Target* target) const override;

        // void add_extra_headers(Target* target);
        std::string format_url(Target* target) const override;

        template <class... Args>
        static MirrorID id(const std::string& host,
                           const std::string& repo_prefix,
                           [[maybe_unused]] Args&&... args)
        {
            return MirrorID(fmt::format("OCIMirror[{}/{}]", host, repo_prefix));
        }

        void set_fn_tag_split_function(
            const split_function_type& func);  // TODO: review, looks like a hack

    private:
        struct AuthCallbackData
        {
            OCIMirror* self;
            Target* target;
            Response* response;
            std::string sha256sum, token, buffer;
        };


        std::map<std::string, std::unique_ptr<AuthCallbackData>> m_path_cb_map;
        std::string m_repo_prefix;
        std::string m_scope;
        std::string m_username;
        std::string m_password;
        split_function_type m_split_func;

        // we copy over the proxy map from the context, otherwise we can't set new
        // proxy options for each curl handle
        proxy_map_type m_proxy_map;

        std::pair<std::string, std::string> split_path_tag(const std::string& path) const;

        AuthCallbackData* get_data(Target* target) const;


        // upload specific functions
        std::string get_digest(const fs::path& p) const;
    };

    struct POWERLOADER_API OCILayer
    {
        std::string mime_type;

        // The OCI Layer can either contain a file or string contents
        std::optional<fs::path> file;
        std::optional<std::string> contents;

        // sha256 digest and size computed in the constructor
        std::string digest;
        std::size_t size;

        // optional annotations that can be added to each layer or config
        std::optional<nlohmann::json> annotations;

        static OCILayer from_file(const std::string& mime_type,
                                  const fs::path& file,
                                  const std::optional<nlohmann::json>& annotations = std::nullopt);

        static OCILayer from_string(const std::string& mime_type,
                                    const std::string& content,
                                    const std::optional<nlohmann::json>& annotations
                                    = std::nullopt);

        Response upload(const Context& ctx,
                        const OCIMirror& mirror,
                        const std::string& reference) const;

        nlohmann::json to_json() const;

    private:
        OCILayer(const std::string& mime_type,
                 const std::optional<fs::path>& path,
                 const std::optional<std::string>& content,
                 const std::optional<nlohmann::json>& annotations = std::nullopt);
    };

    POWERLOADER_API
    Response oci_upload(const Context& ctx,
                        OCIMirror& mirror,
                        const std::string& reference,
                        const std::string& tag,
                        const std::vector<OCILayer>& layers,
                        const std::optional<OCILayer>& config = std::nullopt);

}

#endif
