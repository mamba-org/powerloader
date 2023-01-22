#include <powerloader/context.hpp>

#include <algorithm>
#include <exception>
#include <set>
#include <stdexcept>

#ifdef WITH_ZCHUNK
extern "C"
{
#include <zck.h>
}
#endif

#include <powerloader/mirror.hpp>


namespace powerloader
{
    static std::atomic<bool> is_context_alive{ false };

    Context::Context()
    {
        bool expected = false;
        if (!is_context_alive.compare_exchange_strong(expected, true))
            throw std::runtime_error(
                "powerloader::Context created more than once - instance must be unique");

        cache_dir = fs::absolute(fs::path(".pdcache"));
        if (!fs::exists(cache_dir))
        {
            fs::create_directories(cache_dir);
        }
        set_verbosity(0);
    }

    Context::~Context()
    {
        is_context_alive = false;
    }

    void Context::set_verbosity(int v)
    {
        verbosity = v;
        if (v > 0)
        {
#ifdef WITH_ZCHUNK
            zck_set_log_level(ZCK_LOG_DEBUG);
#endif
            spdlog::set_level(spdlog::level::debug);
        }
        else
        {
            spdlog::set_level(spdlog::level::warn);
        }
    }

    std::string mirror_map_type::to_string() const
    {
        std::string result;
        for (const auto& [mirror_name, mirrors] : *this)
        {
            result += mirror_name + ": [";
            for (const auto& mirror : mirrors)
            {
                result += mirror->id().to_string() + ", ";
            }
            result += "]\n";
        }
        return result;
    }

    mirror_set mirror_map_type::get_mirrors(std::string_view host_name) const
    {
        auto find_it = find(std::string(host_name));
        if (find_it == end())
            return {};

        return find_it->second;
    }

    // Returns true if there are registered mirrors stored here, false if none are.
    bool mirror_map_type::has_mirrors(std::string_view host_name) const
    {
        auto find_it = find(std::string(host_name));
        return find_it != end() && !find_it->second.empty();
    }

    bool mirror_map_type::add_unique_mirror(std::string_view host_name,
                                            std::shared_ptr<Mirror> mirror)
    {
        auto find_it = find(std::string(host_name));
        if (find_it != end())
        {
            auto& mirrors = find_it->second;
            if (details::already_exists(mirror->id(), mirrors))
                return false;
            mirrors.push_back(std::move(mirror));
        }
        else
        {
            (*this)[std::string(host_name)] = { std::move(mirror) };
        }
        return true;
    }

    void mirror_map_type::reset(mirror_map_base new_values)
    {
        if (!details::is_every_mirror_unique_per_host(new_values))
            throw std::invalid_argument("mirror map must have unique mirrors per host name");
        static_cast<mirror_map_base&>(*this) = std::move(new_values);
    }

    namespace details
    {
        bool already_exists(const MirrorID& id, const mirror_set& mirrors)
        {
            for (auto&& mirror : mirrors)
                if (mirror->id() == id)
                    return true;
            return false;
        }

        bool is_every_mirror_unique_per_host(const mirror_map_base& mirrors)
        {
            for (const auto& slot : mirrors)
            {
                std::set<MirrorID> mirrors_ids;  // TODO: replace by flat_set once available.
                for (const auto& mirror : slot.second)
                {
                    auto [_, success] = mirrors_ids.insert(mirror->id());
                    if (!success)
                        return false;
                }
            }
            return true;
        }
    }
}
