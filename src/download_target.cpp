#include "powerloader/context.hpp"
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
                                   const fs::path& destination)
        : m_is_zchunk(ends_with(path, ".zck"))
        , m_path(path)
        , m_base_url(base_url)
        , m_destination_path(destination)
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
            m_p_zck->zck_cache_file = m_destination_path;
        }
#endif
    }

    // Creates a `DownloadTarget` given an URL.
    // When the url is a regular url (with "://"), mirrors will be added to the `Context` for it's host
    // if not already existing.
    std::shared_ptr<DownloadTarget> DownloadTarget::from_url(Context& ctx, const std::string& target_url, const fs::path& destination_path)
    {
        if (contains(target_url, "://"))
        {
            // even when we get a regular URL like `http://test.com/download.tar.gz`
            // we want to create a "mirror" for `http://test.com` to make sure we correctly
            // retry and wait on mirror failures
            URLHandler uh{target_url};
            const std::string url = uh.url();
            const std::string host = uh.host();
            const std::string path = uh.path();
            const std::string mirror_url = url.substr(0, url.size() - path.size());
            const fs::path dst = destination_path.empty() ? fs::path{rsplit(path, "/", 1).back()} : destination_path;

            // Note that `ctx.mirror_map[host]` will create the vector by default at key `host` if it doesnt already exists.
            ctx.mirror_map[host].push_back(std::make_shared<Mirror>(ctx, mirror_url));

            return std::make_shared<DownloadTarget>(path.substr(1, std::string::npos), host, dst);
        }
        else
        {
            const std::vector<std::string> parts = split(target_url, ":");
            if (parts.size() != 2)
            {
                throw std::runtime_error("Not the correct number of : in the url");
            }
            const auto mirror = parts[0];
            const auto path = parts[1];

            fs::path dst
                = destination_path.empty() ? fs::path{rsplit(path, "/", 1).back()} : destination_path;

            // if (!dest_folder.empty())
            //     dst = dest_folder + "/" + dst;

            return std::make_shared<DownloadTarget>(path, mirror, dst);
        }
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
        return fs::exists(m_destination_path) && validate_checksum(m_destination_path);
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
