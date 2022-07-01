#include <powerloader/utils.hpp>
#include <openssl/evp.h>

namespace powerloader
{
    bool is_sig_interrupted()
    {
        return false;
    }

    bool starts_with(const std::string_view& str, const std::string_view& prefix)
    {
        return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
    }

    bool ends_with(const std::string_view& str, const std::string_view& suffix)
    {
        return str.size() >= suffix.size()
               && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
    }

    std::string sha256(const std::string& str) noexcept
    {
        unsigned char hash[32];

        EVP_MD_CTX* mdctx;
        mdctx = EVP_MD_CTX_create();
        EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
        EVP_DigestUpdate(mdctx, str.c_str(), str.size());
        EVP_DigestFinal_ex(mdctx, hash, nullptr);
        EVP_MD_CTX_destroy(mdctx);

        return hex_string(hash, 32);
    }

    std::string string_transform(const std::string_view& input, int (*functor)(int))
    {
        std::string res(input);
        std::transform(
            res.begin(), res.end(), res.begin(), [&](unsigned char c) { return functor(c); });
        return res;
    }

    std::string to_upper(const std::string_view& input)
    {
        return string_transform(input, std::toupper);
    }

    std::string to_lower(const std::string_view& input)
    {
        return string_transform(input, std::tolower);
    }

    bool contains(const std::string_view& str, const std::string_view& sub_str)
    {
        return str.find(sub_str) != std::string::npos;
    }

    std::string sha256sum(const fs::path& path)
    {
        unsigned char hash[32];
        EVP_MD_CTX* mdctx;
        mdctx = EVP_MD_CTX_create();
        EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);

        std::ifstream infile(path, std::ios::binary);
        constexpr std::size_t BUFSIZE = 32768;
        std::vector<char> buffer(BUFSIZE);

        while (infile)
        {
            infile.read(buffer.data(), BUFSIZE);
            size_t count = infile.gcount();
            if (!count)
                break;
            EVP_DigestUpdate(mdctx, buffer.data(), count);
        }

        EVP_DigestFinal_ex(mdctx, hash, nullptr);
        EVP_MD_CTX_destroy(mdctx);

        return hex_string(hash, 32);
    }

    std::string md5sum(const fs::path& path)
    {
        unsigned char hash[16];

        EVP_MD_CTX* mdctx;
        mdctx = EVP_MD_CTX_create();
        EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);

        std::ifstream infile(path, std::ios::binary);
        constexpr std::size_t BUFSIZE = 32768;
        std::vector<char> buffer(BUFSIZE);

        while (infile)
        {
            infile.read(buffer.data(), BUFSIZE);
            size_t count = infile.gcount();
            if (!count)
                break;
            EVP_DigestUpdate(mdctx, buffer.data(), count);
        }

        EVP_DigestFinal_ex(mdctx, hash, nullptr);
        EVP_MD_CTX_destroy(mdctx);

        return hex_string(hash, 16);
    }

    std::pair<std::string, std::string> parse_header(const std::string_view& header)
    {
        auto colon_idx = header.find(':');
        if (colon_idx != std::string_view::npos)
        {
            std::string_view key, value;
            key = header.substr(0, colon_idx);
            colon_idx++;
            // remove spaces
            while (std::isspace(header[colon_idx]))
            {
                ++colon_idx;
            }

            // remove \r\n header ending
            value = header.substr(colon_idx, header.size() - colon_idx - 2);
            // http headers are case insensitive!
            std::string lkey = to_lower(key);

            return std::make_pair(lkey, std::string(value));
        }
        return std::make_pair(std::string(), std::string(header));
    }

    std::string get_env(const char* var)
    {
        const char* val = getenv(var);
        if (!val)
        {
            throw std::runtime_error(std::string("Could not find env var: ") + var);
        }
        return val;
    }

    std::string get_env(const char* var, const std::string& default_value)
    {
        const char* val = getenv(var);
        if (!val)
        {
            return default_value;
        }
        return val;
    }

    std::vector<std::string> split(const std::string_view& input,
                                   const std::string_view& sep,
                                   std::size_t max_split)
    {
        std::vector<std::string> result;
        std::size_t i = 0, j = 0, len = input.size(), n = sep.size();

        while (i + n <= len)
        {
            if (input[i] == sep[0] && input.substr(i, n) == sep)
            {
                if (max_split-- <= 0)
                    break;
                result.emplace_back(input.substr(j, i - j));
                i = j = i + n;
            }
            else
            {
                i++;
            }
        }
        result.emplace_back(input.substr(j, len - j));
        return result;
    }

    std::vector<std::string> rsplit(const std::string_view& input,
                                    const std::string_view& sep,
                                    std::size_t max_split)
    {
        if (max_split == SIZE_MAX)
            return split(input, sep, max_split);

        std::vector<std::string> result;

        std::ptrdiff_t i, j, len = static_cast<std::ptrdiff_t>(input.size()),
                             n = static_cast<std::ptrdiff_t>(sep.size());
        i = j = len;

        while (i >= n)
        {
            if (input[i - 1] == sep[n - 1] && input.substr(i - n, n) == sep)
            {
                if (max_split-- <= 0)
                {
                    break;
                }
                result.emplace_back(input.substr(i, j - i));
                i = j = i - n;
            }
            else
            {
                i--;
            }
        }
        result.emplace_back(input.substr(0, j));
        std::reverse(result.begin(), result.end());

        return result;
    }

    void replace_all(std::string& data, const std::string& search, const std::string& replace)
    {
        replace_all_impl<std::string>(data, search, replace);
    }

    void replace_all(std::wstring& data, const std::wstring& search, const std::wstring& replace)
    {
        replace_all_impl<std::wstring>(data, search, replace);
    }
}
