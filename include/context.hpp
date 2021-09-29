#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <map>

#include "mirror.hpp"

namespace powerloader
{
    class Context
    {
    public:
        bool offline = false;
        int verbosity = 0;
        bool adaptive_mirror_sorting = true;

        std::size_t retry_backoff_factor = 2;
        std::chrono::steady_clock::duration retry_default_timeout = std::chrono::seconds(2);
        std::vector<std::unique_ptr<Mirror>> mirrors;
        std::map<std::string, std::shared_ptr<std::vector<Mirror*>>> mirror_map;

        std::vector<std::string> additional_httpheaders;

        static Context& instance();

        inline void set_verbosity(int v)
        {
            verbosity = v;
        }

    private:
        Context();
        ~Context() = default;
    };

}
