#ifndef PL_UTILS_HPP
#define PL_UTILS_HPP

#include <array>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>
#include <cctype>
#include <vector>

namespace powerloader
{
    namespace fs = std::filesystem;

    bool is_sig_interrupted();
    bool starts_with(const std::string_view& str, const std::string_view& prefix);
    bool ends_with(const std::string_view& str, const std::string_view& suffix);

    template <class B>
    inline std::vector<char> hex_to_bytes(const B& buffer, std::size_t size) noexcept
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
    inline std::vector<char> hex_to_bytes(const B& buffer) noexcept
    {
        return hex_to_bytes(buffer, buffer.size());
    }

    template <class B>
    inline std::string hex_string(const B& buffer, std::size_t size)
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
    inline std::string hex_string(const B& buffer)
    {
        return hex_string(buffer, buffer.size());
    }

    std::string sha256(const std::string& str) noexcept;

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

    std::string string_transform(const std::string_view& input, int (*functor)(int));
    std::string to_upper(const std::string_view& input);
    std::string to_lower(const std::string_view& input);
    bool contains(const std::string_view& str, const std::string_view& sub_str);

    std::string sha256sum(const fs::path& path);
    std::string md5sum(const fs::path& path);

    std::pair<std::string, std::string> parse_header(const std::string_view& header);
    std::string get_env(const char* var);
    std::string get_env(const char* var, const std::string& default_value);

    std::vector<std::string> split(const std::string_view& input,
                                   const std::string_view& sep,
                                   std::size_t max_split = SIZE_MAX);

    std::vector<std::string> rsplit(const std::string_view& input,
                                    const std::string_view& sep,
                                    std::size_t max_split);


    template <class S>
    inline void replace_all_impl(S& data, const S& search, const S& replace)
    {
        std::size_t pos = data.find(search);
        while (pos != std::string::npos)
        {
            data.replace(pos, search.size(), replace);
            pos = data.find(search, pos + replace.size());
        }
    }

    void replace_all(std::string& data, const std::string& search, const std::string& replace);

    void replace_all(std::wstring& data, const std::wstring& search, const std::wstring& replace);
}

#endif
