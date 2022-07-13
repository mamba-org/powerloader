#include <spdlog/fmt/fmt.h>

extern "C"
{
#include <openssl/buffer.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
}

#include <powerloader/mirrors/s3.hpp>
#include <powerloader/mirror.hpp>
#include <powerloader/target.hpp>
#include <powerloader/url.hpp>

namespace powerloader
{
    std::string get_yyyymmdd(const std::chrono::system_clock::time_point& t)
    {
        static constexpr std::size_t yyyymmddlength = sizeof("YYYYMMDD");
        std::time_t t_date = std::chrono::system_clock::to_time_t(t);
        char yyyymmdd[yyyymmddlength];
        strftime(yyyymmdd, yyyymmddlength, "%Y%m%d", gmtime(&t_date));
        return yyyymmdd;
    }

    std::string get_iso8601(const std::chrono::system_clock::time_point& t)
    {
        static constexpr std::size_t iso8601length = sizeof("YYYYMMDDTHHMMSSZ");
        std::time_t t_date = std::chrono::system_clock::to_time_t(t);
        char iso8601[iso8601length];
        std::strftime(iso8601, iso8601length, "%Y%m%dT%H%M%SZ", std::gmtime(&t_date));
        return iso8601;
    }

    /**********************
     * S3CanonicalRequest *
     **********************/

    S3CanonicalRequest::S3CanonicalRequest(const std::string& http_verb,
                                           const URLHandler& uh,
                                           const std::string& sha256sum)
        : http_verb(http_verb)
        , hashed_payload(sha256sum.empty() ? EMPTY_SHA : sha256sum)
        , date(std::chrono::system_clock::now())
    {
        bucket_url = uh.url_without_path();
        resource = uh.path();
        if (resource.size() >= 1 && resource[0] == '/')
        {
            resource = resource.substr(1, std::string::npos);
        }

        init_default_headers();
    }

    void S3CanonicalRequest::init_default_headers()
    {
        URLHandler uh(bucket_url);
        headers["x-amz-date"] = get_iso8601(date);
        // if (s3_session_token != "")
        //     headers["x-amz-security-token"] = s3_session_token;
        headers["x-amz-content-sha256"] = hashed_payload;
        headers["Host"] = uh.host();
        headers["Content-Type"] = "application/octet-stream";
    }

    std::string S3CanonicalRequest::get_signed_headers()
    {
        std::stringstream signed_headers;
        for (auto it = headers.begin(); it != headers.end(); ++it)
            signed_headers << (it == headers.begin() ? "" : ";") << to_lower(it->first);
        return signed_headers.str();
    }

    std::string S3CanonicalRequest::canonical_request()
    {
        std::stringstream canonical_headers, signed_headers;
        for (auto it = headers.begin(); it != headers.end(); ++it)
        {
            canonical_headers << to_lower(it->first) << ":" << it->second << "\n";
        }

        std::stringstream ss;
        ss << http_verb << "\n"
           << "/" << resource << "\n"
           << ""
           << "\n"  // canonical query string
           << canonical_headers.str() << "\n"
           << get_signed_headers() << "\n"
           << hashed_payload;

        return ss.str();
    }

    std::string S3CanonicalRequest::string_to_sign(const std::string& region,
                                                   const std::string& service)
    {
        std::stringstream ss;
        ss << "AWS4-HMAC-SHA256\n"
           << get_iso8601(date) << "\n"
           << get_yyyymmdd(date) << "/" << region << "/" << service << "/aws4_request\n"
           << sha256(canonical_request());
        return ss.str();
    }

    /***************
     * S3Mirror    *
     ***************/

    S3Mirror::S3Mirror(const Context& ctx,
                       const std::string& bucket_url,
                       const std::string& region,
                       const std::string& aws_access_key,
                       const std::string& aws_secret_key)
        : Mirror(ctx, bucket_url)
        , bucket_url(bucket_url)
        , region(region)
        , aws_access_key_id(aws_access_key)
        , aws_secret_access_key(aws_secret_key)
    {
        if (bucket_url.back() == '/')
            this->bucket_url = this->bucket_url.substr(0, this->bucket_url.size() - 1);
    }

    S3Mirror::S3Mirror(const Context& ctx, const std::string& url)
        : Mirror(ctx, url)
    {
    }

    S3Mirror::~S3Mirror() = default;

    bool S3Mirror::authenticate(CURLHandle& handle, const std::string& path)
    {
        return true;
    };

    std::string S3Mirror::format_url(Target* target) const
    {
        return fmt::format("{}/{}", bucket_url, target->target->path());
    }

    bool S3Mirror::needs_preparation(Target* target) const
    {
        return false;
    }

    bool S3Mirror::prepare(Target* target)
    {
        return true;
    }

    std::string s3_calculate_signature(const std::chrono::system_clock::time_point& request_date,
                                       const std::string& secret,
                                       const std::string& region,
                                       const std::string& service,
                                       const std::string& string_to_sign)
    {
        std::string yyyymmdd = get_yyyymmdd(request_date);

        const std::string key1{ "AWS4" + secret };

        unsigned int DateKeyLen = 0;
        unsigned char* DateKey = HMAC(EVP_sha256(),
                                      key1.c_str(),
                                      static_cast<int>(key1.size()),
                                      reinterpret_cast<const unsigned char*>(yyyymmdd.c_str()),
                                      yyyymmdd.size(),
                                      NULL,
                                      &DateKeyLen);

        unsigned int DateRegionKeyLen = 0;
        unsigned char* DateRegionKey = HMAC(EVP_sha256(),
                                            DateKey,
                                            DateKeyLen,
                                            reinterpret_cast<const unsigned char*>(region.c_str()),
                                            region.size(),
                                            NULL,
                                            &DateRegionKeyLen);

        ;
        unsigned int DateRegionServiceKeyLen = 0;
        unsigned char* DateRegionServiceKey
            = HMAC(EVP_sha256(),
                   DateRegionKey,
                   DateRegionKeyLen,
                   reinterpret_cast<const unsigned char*>(service.c_str()),
                   service.size(),
                   NULL,
                   &DateRegionServiceKeyLen);

        const std::string AWS4_REQUEST{ "aws4_request" };
        unsigned int SigningKeyLen = 0;
        unsigned char* SigningKey
            = HMAC(EVP_sha256(),
                   DateRegionServiceKey,
                   DateRegionServiceKeyLen,
                   reinterpret_cast<const unsigned char*>(AWS4_REQUEST.c_str()),
                   AWS4_REQUEST.size(),
                   NULL,
                   &SigningKeyLen);

        unsigned int SignatureLen = 0;
        unsigned char* Signature
            = HMAC(EVP_sha256(),
                   SigningKey,
                   SigningKeyLen,
                   reinterpret_cast<const unsigned char*>(string_to_sign.c_str()),
                   string_to_sign.size(),
                   NULL,
                   &SignatureLen);

        return hex_string(Signature, SHA256_DIGEST_LENGTH);
    }

    std::vector<std::string> S3Mirror::get_auth_headers(S3CanonicalRequest& request) const
    {
        std::vector<std::string> headers;

        const std::string signature = s3_calculate_signature(request.date,
                                                             aws_secret_access_key,
                                                             region,
                                                             "s3",
                                                             request.string_to_sign(region, "s3"));

        std::stringstream authorization_header;
        authorization_header << "AWS4-HMAC-SHA256 Credential=" << aws_access_key_id << "/"
                             << get_yyyymmdd(request.date) << "/" << region << "/s3/aws4_request, "
                             << "SignedHeaders=" << request.get_signed_headers() << ", "
                             << "Signature=" << signature;

        for (auto& [key, header] : request.headers)
        {
            // if (key == "x-amz-content-sha256") continue;
            headers.push_back(fmt::format("{}: {}", key, header));
        }
        headers.push_back(fmt::format("Authorization: {}", authorization_header.str()));
        return headers;
    }

    std::vector<std::string> S3Mirror::get_auth_headers(const std::string& path) const
    {
        URLHandler uh(fmt::format("{}/{}", bucket_url, path));
        S3CanonicalRequest req_data("GET", uh);
        return get_auth_headers(req_data);
    }
}
