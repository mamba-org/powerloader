#include <spdlog/fmt/fmt.h>

#include "mirror.hpp"
#include "target.hpp"

namespace powerloader
{
    std::string get_yyyymmdd(const std::chrono::system_clock::time_point& t);
    std::string get_iso8601(const std::chrono::system_clock::time_point& t);

    // TODO replace with proper URL parsing
    std::string get_host(std::string& url);

    struct S3CanonicalRequest
    {
        std::string http_verb;
        std::string resource;
        std::string bucket_url;
        std::map<std::string, std::string> headers;
        std::map<std::string, std::string> query_string;
        std::string hashed_payload;

        std::chrono::system_clock::time_point date;

        inline S3CanonicalRequest(const std::string& http_verb,
                                  const std::string& bucket_url,
                                  const std::string& resource = "",
                                  const std::string& sha256sum = "")
            : http_verb(http_verb)
            , bucket_url(bucket_url)
            , resource(resource)
            , hashed_payload(sha256sum.empty() ? EMPTY_SHA : sha256sum)
            , date(std::chrono::system_clock::now())
        {
            init_default_headers();
        }

        void init_default_headers();
        std::string get_signed_headers();
        std::string canonical_request();

        std::string string_to_sign(const std::string& region, const std::string& service);
    };

    // https://gist.github.com/mmaday/c82743b1683ce4d27bfa6615b3ba2332
    struct S3Mirror : public Mirror
    {
        std::string bucket_url;
        std::string aws_access_key_id = "AKIAIOSFODNN7EXAMPLE";
        std::string aws_secret_access_key = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
        std::string region = "eu-central-1";

        inline S3Mirror(const std::string& bucket_url,
                        const std::string& region,
                        const std::string& aws_access_key,
                        const std::string& aws_secret_key)
            : bucket_url(bucket_url)
            , region(region)
            , aws_access_key_id(aws_access_key)
            , aws_secret_access_key(aws_secret_key)
            , Mirror(bucket_url)
        {
            if (bucket_url.back() == '/')
                this->bucket_url = this->bucket_url.substr(0, this->bucket_url.size() - 1);
        }

        inline S3Mirror(const std::string& url)
            : Mirror(url)
        {
        }

        inline bool authenticate(CURLHandle& handle, const std::string& path)
        {
            return true;
        };

        inline std::string format_url(Target* target)
        {
            return fmt::format("{}/{}", bucket_url, target->target->path);
        }

        inline bool need_preparation(Target* target)
        {
            return false;
        }

        inline bool prepare(Target* target)
        {
            return true;
        }

        std::string calculate_signature(const std::chrono::system_clock::time_point& request_date,
                                        const std::string& secret,
                                        const std::string& region,
                                        const std::string& service,
                                        const std::string& string_to_sign);

        std::vector<std::string> get_auth_headers(const std::string& path);
        std::vector<std::string> get_auth_headers(S3CanonicalRequest& request);
    };

    Response s3_upload(S3Mirror& mirror, const std::string& path, const fs::path& file);
}
