#include <spdlog/fmt/fmt.h>

#include <powerloader/mirror.hpp>
#include <powerloader/mirrors/oci.hpp>
#include "powerloader/mirrorid.hpp"
#include "target.hpp"

namespace powerloader
{
    Mirror::~Mirror() = default;

    void Mirror::change_max_ranges(int new_value)
    {
        // TODO: add some checks here.
        m_stats.max_ranges = new_value;
    }

    bool Mirror::need_wait_for_retry() const
    {
        return m_retry_counter != 0 && m_next_retry > std::chrono::system_clock::now();
    }

    bool Mirror::has_running_transfers() const
    {
        return m_stats.running_transfers > 0;
    }

    void Mirror::set_allowed_parallel_connections(int max_allowed_parallel_connections)
    {
        m_stats.allowed_parallel_connections = max_allowed_parallel_connections;
    }

    void Mirror::increase_running_transfers()
    {
        m_stats.running_transfers++;
        if (m_stats.max_tried_parallel_connections < m_stats.running_transfers)
        {
            m_stats.max_tried_parallel_connections = m_stats.running_transfers;
        }
    }

    bool Mirror::is_parallel_connections_limited_and_reached() const
    {
        return m_stats.allowed_parallel_connections != -1
               && m_stats.running_transfers >= m_stats.allowed_parallel_connections;
    }

    void Mirror::update_statistics(bool transfer_success)
    {
        m_stats.running_transfers--;
        if (transfer_success)
        {
            m_stats.successful_transfers++;
        }
        else
        {
            m_stats.failed_transfers++;
            if (m_stats.failed_transfers == 1 || m_next_retry < std::chrono::system_clock::now())
            {
                m_retry_counter++;
                m_retry_wait_seconds = m_retry_wait_seconds * m_retry_backoff_factor;
                m_next_retry = std::chrono::system_clock::now() + m_retry_wait_seconds;
            }
        }
    }

    double Mirror::rank() const
    {
        double rank = -1.0;

        const int finished_transfers = m_stats.count_finished_transfers();

        if (finished_transfers < 3)
            return rank;  // Do not judge too early

        rank = m_stats.successful_transfers / static_cast<double>(finished_transfers);

        return rank;
    }

    bool Mirror::prepare(Target*)
    {
        m_state = MirrorState::READY;
        return true;
    }

    bool Mirror::prepare(const std::string&, CURLHandle&)
    {
        m_state = MirrorState::READY;
        return true;
    }

    bool Mirror::needs_preparation(Target*) const
    {
        return false;
    }

    bool Mirror::authenticate(CURLHandle&, const std::string&)
    {
        return true;
    }

    std::vector<std::string> Mirror::get_auth_headers(const std::string&) const
    {
        return {};
    }

    std::string Mirror::format_url(Target* target) const
    {
        return join_url(m_url, target->target().path());
    }

    /** Sort mirrors. Penalize the error ones.
     * In fact only move the current finished mirror forward or backward
     * by one position.
     * @param mirrors   GSList of mirrors (order of list elements won't be
     *                  changed, only data pointers)
     * @param mirror    Mirror of just finished transfer
     * @param success   Was download from the mirror successful
     * @param serious   If success is FALSE, serious mean that error was serious
     *                  (like connection timeout), and the mirror should be
     *                  penalized more that usual.
     */
    bool sort_mirrors(std::vector<std::shared_ptr<Mirror>>& mirrors,
                      const std::shared_ptr<Mirror>& mirror,
                      bool success,
                      bool serious)
    {
        assert(mirror);

        if (mirrors.size() == 1)
            return true;

        auto it = std::find(mirrors.begin(), mirrors.end(), mirror);

        // no penalization, mirror is already last
        if (!success && (it + 1) == mirrors.end())
            return true;

        // Bonus not needed - Mirror is already the first one
        if (success && it == mirrors.begin())
            return true;

        // Serious errors
        if (serious && mirror->stats().successful_transfers == 0)
        {
            // Mirror that encounter a serious error and has no successful
            // transfers should be moved at the end of the list
            // (such mirror is probably down/broken/buggy)

            // TODO should we really _swap_ here or rather move that one mirror down and shuffle all
            // others?!
            std::iter_swap(it, mirrors.end() - 1);
            spdlog::info("Mirror {} was moved to the end", mirror->url());
            return true;
        }

        // Calculate ranks
        double rank_cur = mirror->rank();
        // Too early to judge
        if (rank_cur < 0.0)
            return true;

        if (!success)
        {
            // Penalize
            double rank_next = (*(it + 1))->rank();
            if (rank_next < 0.0 || rank_next > rank_cur)
            {
                std::iter_swap(it, it + 1);
                spdlog::info("Mirror {} was penalized", mirror->url());
            }
        }
        else
        {
            // Bonus
            double rank_prev = (*(it - 1))->rank();
            if (rank_prev < rank_cur)
            {
                std::iter_swap(it, it - 1);
                spdlog::info("Mirror {} was awarded", mirror->url());
            }
        }

        return true;
    }

    bool HTTPMirror::prepare(Target* target)
    {
        return true;
    }

    bool HTTPMirror::prepare(const std::string& path, CURLHandle& handle)
    {
        return true;
    }

    bool HTTPMirror::needs_preparation(Target* target) const
    {
        return false;
    }

    bool HTTPMirror::authenticate(CURLHandle& handle, const std::string& path)
    {
        if (!m_auth_password.empty())
        {
            spdlog::warn(
                "Setting HTTP authentication for {} to {}:{}", path, m_auth_user, m_auth_password);
            handle.setopt(CURLOPT_USERNAME, m_auth_user.c_str());
            handle.setopt(CURLOPT_PASSWORD, m_auth_password.c_str());
        }
        return true;
    }

    void HTTPMirror::set_auth(const std::string& user, const std::string& password)
    {
        m_auth_user = user;
        m_auth_password = password;
    }


    std::vector<std::string> HTTPMirror::get_auth_headers(const std::string& path) const
    {
        return {};
    }

    std::string HTTPMirror::format_url(Target* target) const
    {
        return join_url(url(), target->target().path());
    }

}
