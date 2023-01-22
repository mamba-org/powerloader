#ifndef POWERLOADER_S3_HPP
#define POWERLOADER_S3_HPP

#include <spdlog/fmt/fmt.h>

#include <powerloader/export.hpp>
#include <powerloader/mirror.hpp>
#include <powerloader/url.hpp>

namespace powerloader
{

    class Target;

    struct POWERLOADER_API S3CanonicalRequest
    {
        std::string http_verb;
        std::string resource;
        std::string bucket_url;
        std::map<std::string, std::string> headers;
        std::map<std::string, std::string> query_string;
        std::string hashed_payload;

        std::chrono::system_clock::time_point date;

        S3CanonicalRequest(const std::string& http_verb,
                           const URLHandler& uh,
                           const std::string& sha256sum = "");

        void init_default_headers();
        std::string get_signed_headers();
        std::string canonical_request();

        std::string string_to_sign(const std::string& region, const std::string& service);
    };

    // https://gist.github.com/mmaday/c82743b1683ce4d27bfa6615b3ba2332
    class POWERLOADER_API S3Mirror : public Mirror
    {
    public:
        S3Mirror(const Context& ctx,
                 const std::string& bucket_url,
                 const std::string& region,
                 const std::string& aws_access_key,
                 const std::string& aws_secret_key);

        ~S3Mirror();

        static MirrorID id(const std::string& bucket_url, const std::string& region)
        {
            return MirrorID(fmt::format("S3Mirror[{}/{}]", bucket_url, region));
        }

        std::vector<std::string> get_auth_headers(const std::string& path) const override;
        std::vector<std::string> get_auth_headers(S3CanonicalRequest& request) const;

    private:
        std::string bucket_url;
        std::string region = "eu-central-1";
        std::string aws_access_key_id = "AKIAIOSFODNN7EXAMPLE";
        std::string aws_secret_access_key = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";

        bool authenticate(CURLHandle& handle, const std::string& path) override;
        std::string format_url(Target* target) const override;
        bool needs_preparation(Target* target) const override;
        bool prepare(Target* target) override;
    };


    POWERLOADER_API
    std::string s3_calculate_signature(const std::chrono::system_clock::time_point& request_date,
                                       const std::string& secret,
                                       const std::string& region,
                                       const std::string& service,
                                       const std::string& string_to_sign);

    POWERLOADER_API
    Response s3_upload(const Context& ctx,
                       S3Mirror& mirror,
                       const std::string& path,
                       const fs::path& file);
}

#endif
