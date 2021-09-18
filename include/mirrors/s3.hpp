#include "mirror.hpp"
#include "target.hpp"

std::string get_yyyymmdd(const std::chrono::system_clock::time_point &t)
{
    static constexpr std::size_t yyyymmddlength = sizeof("YYYYMMDD");
    std::time_t t_date = std::chrono::system_clock::to_time_t(t);
    char yyyymmdd[yyyymmddlength];
    strftime(yyyymmdd, yyyymmddlength, "%Y%m%d", gmtime(&t_date));
    return yyyymmdd;
}

std::string get_iso8601(const std::chrono::system_clock::time_point &t)
{
    static constexpr std::size_t iso8601length = sizeof("YYYYMMDDTHHMMSSZ");
    std::time_t t_date = std::chrono::system_clock::to_time_t(t);
    char iso8601[iso8601length];
    std::strftime(iso8601, iso8601length, "%Y%m%dT%H%M%SZ", std::gmtime(&t_date));
    return iso8601;
}

struct S3CanonicalRequest
{
    std::string http_verb;
    std::string resource;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_string;
    // this is the hashed payload of an empty string which is the default for GET requests
    std::string hashed_payload = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    std::chrono::system_clock::time_point date;

    S3CanonicalRequest(const std::string &http_verb, const std::string &resource = "")
        // : http_verb(http_verb), resource(resource), date(std::chrono::system_clock::time_point() + std::chrono::seconds(1631748220)) //, date(std::chrono::system_clock::now())
        : http_verb(http_verb), resource(resource), date(std::chrono::system_clock::now())
    {
        init_default_headers();
    }

    void init_default_headers()
    {
        headers["x-amz-date"] = get_iso8601(date);
        // if (s3_session_token != "")
        //     headers["x-amz-security-token"] = s3_session_token;
        headers["x-amz-content-sha256"] = hashed_payload;
        headers["Host"] = "wolfsuperbuckettest.s3.amazonaws.com";
        headers["Content-Type"] = "application/octet-stream";
        // headers["Accept"] = "*/*";
    }

    std::string get_signed_headers()
    {
        std::stringstream signed_headers;
        for (auto it = headers.begin(); it != headers.end(); ++it)
            signed_headers << (it == headers.begin() ? "" : ";") << to_lower(it->first);
        return signed_headers.str();
    }

    std::string canonical_request()
    {
        std::stringstream canonical_headers, signed_headers;
        for (auto it = headers.begin(); it != headers.end(); ++it)
        {
            canonical_headers << to_lower(it->first) << ":" << it->second << "\n";
        }

        std::stringstream ss;
        ss << http_verb << "\n"
           << "/" << resource << "\n"
           << "" << "\n" // canonical query string
           << canonical_headers.str() << "\n"
           << get_signed_headers() << "\n"
           << hashed_payload;

        return ss.str();
    }

    std::string string_to_sign(//const std::chrono::system_clock::time_point& date,
                               const std::string& region,
                               const std::string& service)
    {
        std::stringstream ss;
        ss << "AWS4-HMAC-SHA256\n"
           << get_iso8601(date) << "\n"
           << get_yyyymmdd(date) << "/" << region << "/" << service << "/aws4_request\n"
           << sha256(canonical_request());
        return ss.str();
    }
};

// https://gist.github.com/mmaday/c82743b1683ce4d27bfa6615b3ba2332
struct S3Mirror : public Mirror
{
    std::string bucket_url;
    std::string aws_access_key_id = "AKIAIOSFODNN7EXAMPLE";
    std::string aws_secret_access_key = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";

    std::string region = "eu-central-1";

    S3Mirror(const std::string& bucket_url, const std::string& aws_access_key, const std::string& aws_secret_key)
        : bucket_url(bucket_url), aws_access_key_id(aws_access_key), aws_secret_access_key(aws_secret_key), Mirror(bucket_url)
    {
    }

    inline std::string calculate_signature(
        const std::chrono::system_clock::time_point &request_date,
        const std::string &secret,
        const std::string &region,
        const std::string &service,
        const std::string &string_to_sign)
    {
        std::string yyyymmdd = get_yyyymmdd(request_date);

        const std::string key1{"AWS4" + secret};

        unsigned char *DateKey;
        unsigned int DateKeyLen;
        DateKey = HMAC(EVP_sha256(), key1.c_str(), key1.size(),
                       reinterpret_cast<const unsigned char *>(yyyymmdd.c_str()), yyyymmdd.size(), NULL, &DateKeyLen);

        unsigned char *DateRegionKey;
        unsigned int DateRegionKeyLen;
        DateRegionKey = HMAC(EVP_sha256(), DateKey, DateKeyLen,
                             reinterpret_cast<const unsigned char *>(region.c_str()),
                             region.size(), NULL, &DateRegionKeyLen);


        unsigned char *DateRegionServiceKey;
        unsigned int DateRegionServiceKeyLen;
        DateRegionServiceKey = HMAC(EVP_sha256(), DateRegionKey, DateRegionKeyLen,
                                    reinterpret_cast<const unsigned char *>(service.c_str()),
                                    service.size(), NULL, &DateRegionServiceKeyLen);

        const std::string AWS4_REQUEST{"aws4_request"};
        unsigned char *SigningKey;
        unsigned int SigningKeyLen;
        SigningKey = HMAC(EVP_sha256(), DateRegionServiceKey, DateRegionServiceKeyLen,
                          reinterpret_cast<const unsigned char *>(AWS4_REQUEST.c_str()),
                          AWS4_REQUEST.size(), NULL, &SigningKeyLen);

        unsigned char *Signature;
        unsigned int SignatureLen;
        Signature = HMAC(EVP_sha256(), SigningKey, SigningKeyLen,
                         reinterpret_cast<const unsigned char *>(string_to_sign.c_str()),
                         string_to_sign.size(), NULL, &SignatureLen);

        return hex_string(Signature, SHA256_DIGEST_LENGTH);
    }

    inline S3Mirror(const std::string &url)
        : Mirror(url)
    {
    }

    inline bool need_preparation(Target *target)
    {
        return false;
    }

    // authenticate per target, and authentication state
    // is also dependent on each target unfortunately?!
    inline bool prepare(Target *target)
    {
        return true;
    }

    inline void add_extra_headers(Target *target)
    {
        // target->setopt(CURLOPT_VERBOSE, 1);
        S3CanonicalRequest req_data(
            "GET",
            target->target->path
        );

        std::string signature = calculate_signature(
            req_data.date,
            aws_secret_access_key,
            region,
            "s3",
            req_data.string_to_sign(region, "s3"));

        std::stringstream authorization_header;
        authorization_header << "AWS4-HMAC-SHA256 Credential=" << aws_access_key_id << "/"
                             << get_yyyymmdd(req_data.date) << "/" << region << "/s3/aws4_request, "
                             << "SignedHeaders=" << req_data.get_signed_headers() << ", "
                             << "Signature=" << signature;

        for (auto& [key, header] : req_data.headers)
        {
            target->add_header(fmt::format("{}: {}", key, header));
        }
        target->add_header(fmt::format("Authorization: {}", authorization_header.str()));
    }

    inline std::string format_url(Target *target)
    {
        return fmt::format("{}/{}", bucket_url, target->target->path);
    }
};