#include <spdlog/fmt/fmt.h>

#include "mirror.hpp"
#include "mirrors/oci.hpp"
#include "target.hpp"

namespace powerloader
{
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

    std::string Mirror::format_url(Target* target)
    {
        return fmt::format("{}/{}", url, target->target->path);
    }

    std::vector<std::string> Mirror::get_auth_headers(const std::string& path)
    {
        return {};
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
