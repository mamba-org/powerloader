#ifndef POWERLOADER_OCI_HPP
#define POWERLOADER_OCI_HPP

#include <optional>
#include <spdlog/fmt/fmt.h>

#include <powerloader/export.hpp>
#include <powerloader/mirror.hpp>

namespace powerloader
{
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

        void set_fn_tag_split_function(const split_function_type& func);


        std::string get_repo(const std::string& repo) const;
        std::string get_auth_url(const std::string& repo, const std::string& scope) const;
        std::string get_manifest_url(const std::string& repo, const std::string& reference) const;
        std::string get_preupload_url(const std::string& repo) const;

        std::vector<std::string> get_auth_headers(const std::string& path) const override;

        // authenticate per target, and authentication state
        // is also dependent on each target unfortunately?!
        bool prepare(const std::string& path, CURLHandle& handle) override;
        bool need_auth() const;
        bool need_preparation(Target* target) override;

        // void add_extra_headers(Target* target);
        std::string format_url(Target* target) override;

        // upload specific functions
        std::string get_digest(const fs::path& p) const;

    private:
        struct AuthCallbackData
        {
            OCIMirror* self;
            Target* target;
            Response response;
            std::string sha256sum, token, buffer;
        };


        std::map<std::string, std::unique_ptr<AuthCallbackData>> m_path_cb_map;
        std::string m_repo_prefix;
        std::string m_scope;
        std::string m_username;
        std::string m_password;
        split_function_type m_split_func;



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
