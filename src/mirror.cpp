#include <spdlog/fmt/fmt.h>

#include "mirror.hpp"
#include "mirrors/oci.hpp"
#include "target.hpp"

namespace powerloader
{
    Mirror::Mirror(const std::string& url)
        : url(url)
        , preference(0)
        , protocol(Protocol::kHTTP)
    {
        if (url.back() == '/')
            this->url = this->url.substr(0, this->url.size() - 1);

        auto& ctx = Context::instance();
        if (ctx.max_downloads_per_mirror > 0)
        {
            allowed_parallel_connections = ctx.max_downloads_per_mirror;
        }
    }

    bool Mirror::need_wait_for_retry() const
    {
        return retry_counter != 0 && next_retry > std::chrono::system_clock::now();
    }

    bool Mirror::has_running_transfers() const
    {
        return running_transfers > 0;
    }

    void Mirror::set_allowed_parallel_connections(int max_allowed_parallel_connections)
    {
        allowed_parallel_connections = max_allowed_parallel_connections;
    }

    void Mirror::increase_running_transfers()
    {
        running_transfers++;
        if (max_tried_parallel_connections < running_transfers)
        {
            max_tried_parallel_connections = running_transfers;
        }
    }

    bool Mirror::is_parallel_connections_limited_and_reached() const
    {
        return allowed_parallel_connections != -1
               && running_transfers >= allowed_parallel_connections;
    }

    void Mirror::update_statistics(bool transfer_success)
    {
        running_transfers--;
        if (transfer_success)
        {
            successful_transfers++;
        }
        else
        {
            failed_transfers++;
            if (failed_transfers == 1 || next_retry < std::chrono::system_clock::now())
            {
                retry_counter++;
                retry_wait_seconds = retry_wait_seconds * retry_backoff_factor;
                next_retry = std::chrono::system_clock::now() + retry_wait_seconds;
            }
        }
    }

    double Mirror::rank() const
    {
        double rank = -1.0;

        int successful = successful_transfers;
        int failed = failed_transfers;
        int finished_transfers = successful + failed;

        if (finished_transfers < 3)
            return rank;  // Do not judge too early

        rank = successful / (double) finished_transfers;

        return rank;
    }

    bool Mirror::prepare(Target* target)
    {
        state = MirrorState::READY;
        return true;
    }

    bool Mirror::prepare(const std::string& path, CURLHandle& handle)
    {
        state = MirrorState::READY;
        return true;
    }

    bool Mirror::need_preparation(Target* target)
    {
        return false;
    }

    bool Mirror::authenticate(CURLHandle& handle, const std::string& path)
    {
        return true;
    }

    std::vector<std::string> Mirror::get_auth_headers(const std::string& path) const
    {
        return {};
    }

    std::string Mirror::format_url(Target* target)
    {
        return fmt::format("{}/{}", url, target->target->path);
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
        if (serious && mirror->successful_transfers == 0)
        {
            // Mirror that encounter a serious error and has no successful
            // transfers should be moved at the end of the list
            // (such mirror is probably down/broken/buggy)

            // TODO should we really _swap_ here or rather move that one mirror down and shuffle all
            // others?!
            std::iter_swap(it, mirrors.end() - 1);
            spdlog::info("Mirror {} was moved to the end", mirror->url);
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
                spdlog::info("Mirror {} was penalized", mirror->url);
            }
        }
        else
        {
            // Bonus
            double rank_prev = (*(it - 1))->rank();
            if (rank_prev < rank_cur)
            {
                std::iter_swap(it, it - 1);
                spdlog::info("Mirror {} was awarded", mirror->url);
            }
        }

        return true;
    }
}
