#ifndef POWERLOADER_UTILS_HPP
#define POWERLOADER_UTILS_HPP

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

#include <powerloader/export.hpp>

namespace powerloader
{
    namespace fs = std::filesystem;

    POWERLOADER_API bool is_sig_interrupted();
    POWERLOADER_API bool starts_with(const std::string_view& str, const std::string_view& prefix);
    POWERLOADER_API bool ends_with(const std::string_view& str, const std::string_view& suffix);

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

    POWERLOADER_API std::string sha256(const std::string& str) noexcept;

    class POWERLOADER_API download_error : public std::runtime_error
    {
    public:
        download_error(const std::string& what = "download error", bool serious = false)
            : std::runtime_error(what)
            , m_serious(serious)
        {
        }
        bool m_serious;
    };

    class fatal_download_error : public std::runtime_error
    {
    public:
        fatal_download_error(const std::string& what = "fatal download error")
            : std::runtime_error(what)
        {
        }
    };

    POWERLOADER_API std::string string_transform(const std::string_view& input,
                                                 int (*functor)(int));
    POWERLOADER_API std::string to_upper(const std::string_view& input);
    POWERLOADER_API std::string to_lower(const std::string_view& input);
    POWERLOADER_API bool contains(const std::string_view& str, const std::string_view& sub_str);

    POWERLOADER_API std::string sha256sum(const fs::path& path);
    POWERLOADER_API std::string md5sum(const fs::path& path);

    POWERLOADER_API std::pair<std::string, std::string> parse_header(
        const std::string_view& header);
    POWERLOADER_API std::string get_env(const char* var);
    POWERLOADER_API std::string get_env(const char* var, const std::string& default_value);

    POWERLOADER_API
    std::vector<std::string> split(const std::string_view& input,
                                   const std::string_view& sep,
                                   std::size_t max_split = SIZE_MAX);

    POWERLOADER_API
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

    POWERLOADER_API
    void replace_all(std::string& data, const std::string& search, const std::string& replace);

    POWERLOADER_API
    void replace_all(std::wstring& data, const std::wstring& search, const std::wstring& replace);

    // Removes duplicate values (compared using `==`) from a sequence container.
    // This will change the order of the elements.
    // Returns the new end iterator for the container.
    template<typename SequenceContainer> // TODO: use a concept once C++20 is available
    auto erase_duplicates(SequenceContainer&& container)
    {
        // TODO: use ranges once c++20 is available
        std::stable_sort(begin(container), end(container));
        auto new_end = std::unique(begin(container), end(container));
        return container.erase(new_end, container.end());
    }

}

#endif
