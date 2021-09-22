#pragma once

#include <array>
#include <algorithm>
#include <fmt/core.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

extern "C"
{
#include <openssl/sha.h>
}

#define LR_DOWNLOADER_MAXIMAL_RESUME_COUNT 10

#define EMPTY_SHA "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

inline bool
is_sig_interrupted()
{
    return false;
}

inline bool
starts_with(const std::string_view& str, const std::string_view& prefix)
{
    return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

inline bool
ends_with(const std::string_view& str, const std::string_view& suffix)
{
    return str.size() >= suffix.size()
           && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

template <class B>
inline std::vector<char>
hex_to_bytes(const B& buffer, std::size_t size) noexcept
{
    std::vector<char> res;
    if (size % 2 != 0)
        return res;

    std::string extract;
    for (auto pos = buffer.cbegin(); pos < buffer.cend(); pos += 2)
    {
        extract.assign(pos, pos + 2);
        res.push_back(std::stoi(extract, nullptr, 16));
    }
    return res;
}

template <class B>
inline std::vector<char>
hex_to_bytes(const B& buffer) noexcept
{
    return hex_to_bytes(buffer, buffer.size());
}

template <class B>
inline std::string
hex_string(const B& buffer, std::size_t size)
{
    std::ostringstream oss;
    oss << std::hex;
    for (std::size_t i = 0; i < size; ++i)
    {
        oss << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);
    }
    return oss.str();
}

template <class B>
inline std::string
hex_string(const B& buffer)
{
    return hex_string(buffer, buffer.size());
}

inline std::string
sha256(const std::string& str) noexcept
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    return hex_string(hash, SHA256_DIGEST_LENGTH);
}

// template <class Arg1, class Args...>
// inline void print(const char *message_template, const Arg1 &message_arg1,
// const Args &...message_args)
// {
//     printf(message_template, message_arg1, message_args...);
// }

#define p_print(...) printf(__VA_ARGS__)
#define p_debug(...) printf(__VA_ARGS__)

#define pfdebug(...) std::cout << fmt::format(__VA_ARGS__) << std::endl

class download_error : public std::runtime_error
{
public:
    download_error(const std::string& what = "download error", bool serious = false)
        : std::runtime_error(what)
        , serious(serious)
    {
    }
    bool serious;
};

class fatal_download_error : public std::runtime_error
{
public:
    fatal_download_error(const std::string& what = "fatal download error")
        : std::runtime_error(what)
    {
    }
};

inline std::string
string_transform(const std::string_view& input, int (*functor)(int))
{
    std::string res(input);
    std::transform(
        res.begin(), res.end(), res.begin(), [&](unsigned char c) { return functor(c); });
    return res;
}

inline std::string
to_upper(const std::string_view& input)
{
    return string_transform(input, std::toupper);
}

inline std::string
to_lower(const std::string_view& input)
{
    return string_transform(input, std::tolower);
}

inline bool
contains(const std::string_view& str, const std::string_view& sub_str)
{
    return str.find(sub_str) != std::string::npos;
}

inline std::string
sha256sum(const std::string& path)
{
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash;

    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    std::ifstream infile(path, std::ios::binary);

    constexpr std::size_t BUFSIZE = 32768;
    std::vector<char> buffer(BUFSIZE);

    while (infile)
    {
        infile.read(buffer.data(), BUFSIZE);
        size_t count = infile.gcount();
        if (!count)
            break;
        SHA256_Update(&sha256, buffer.data(), count);
    }

    SHA256_Final(hash.data(), &sha256);

    return hex_string(hash);
}

inline std::pair<std::string, std::string>
parse_header(const std::string_view& header)
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

inline std::string
get_env(const char* var)
{
    const char* val = getenv(var);
    if (!val)
    {
        throw std::runtime_error(std::string("Could not find env var: ") + var);
    }
    return val;
}

inline std::vector<std::string>
split(const std::string_view& input, const std::string_view& sep, std::size_t max_split = SIZE_MAX)
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

inline std::vector<std::string>
rsplit(const std::string_view& input, const std::string_view& sep, std::size_t max_split)
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
