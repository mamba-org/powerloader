#include <powerloader/download_target.hpp>

#ifdef WITH_ZCHUNK
#include "zck.hpp"
#endif

namespace powerloader
{

#ifndef WITH_ZCHUNK
    struct zck_target
    {
    };
#endif

    DownloadTarget::DownloadTarget(const std::string& path,
                                   const std::string& base_url,
                                   const fs::path& filename)
        : m_is_zchunk(ends_with(path, ".zck"))
        , m_path(path)
        , m_base_url(base_url)
        , m_filename(filename)
    {
        if (path.find("://") != std::string::npos)
        {
            m_complete_url = path;
        }
        else if (base_url.find("://") != std::string::npos)
        {
            m_complete_url = join_url(base_url, path);
        }

#if WITH_ZCHUNK
        if (m_is_zchunk)
        {
            m_p_zck = std::make_unique<zck_target>();
            m_p_zck->zck_cache_file = filename;
        }
#endif
    }

    DownloadTarget::~DownloadTarget() = default;

    bool DownloadTarget::has_complete_url() const
    {
        return !m_complete_url.empty();
    }

    bool DownloadTarget::validate_checksum(const fs::path& path)
    {
        if (m_checksums.empty())
            return false;

        auto findchecksum = [&](const ChecksumType& t) -> Checksum*
        {
            for (auto& cs : m_checksums)
            {
                if (cs.type == t)
                    return &cs;
            }
            return nullptr;
        };

        Checksum* cs;
        if ((cs = findchecksum(ChecksumType::kSHA256)))
        {
            auto sum = sha256sum(path);

            if (sum != cs->checksum)
            {
                spdlog::error("SHA256 sum of downloaded file is wrong.\nIs {}. Should be {}",
                              sum,
                              cs->checksum);
                return false;
            }
            return true;
        }
        else if ((cs = findchecksum(ChecksumType::kSHA1)))
        {
            spdlog::error("Checking SHA1 sum not implemented!");
            return false;
        }
        else if ((cs = findchecksum(ChecksumType::kMD5)))
        {
            spdlog::info("Checking MD5 sum");
            auto sum = md5sum(path);
            if (sum != cs->checksum)
            {
                spdlog::error(
                    "MD5 sum of downloaded file is wrong.\nIs {}. Should be {}", sum, cs->checksum);
                return false;
            }
            return true;
        }
        return false;
    }

    bool DownloadTarget::already_downloaded()
    {
        if (m_checksums.empty())
            return false;
        return fs::exists(m_filename) && validate_checksum(m_filename);
    }

    void DownloadTarget::set_error(const DownloaderError& err)
    {
        m_error = std::make_unique<DownloaderError>(err);
    }

    void DownloadTarget::set_cache_options(const CacheControl& cache_control)
    {
        m_cache_control = cache_control;
    }

    void DownloadTarget::add_handle_options(CURLHandle& handle)
    {
        auto to_header = [](const std::string& key, const std::string& value)
        { return std::string(key + ": " + value); };

        if (m_cache_control.etag.size())
        {
            handle.add_header(to_header("If-None-Match", m_cache_control.etag));
        }
        if (m_cache_control.last_modified.size())
        {
            handle.add_header(to_header("If-Modified-Since", m_cache_control.last_modified));
        }
    }
}
