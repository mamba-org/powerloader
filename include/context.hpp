#pragma once

#include <vector>
#include <string>
#include <map>

#include "mirror.hpp"

class Context
{
public:
    bool offline = false;
    int verbosity = 0;

    std::vector<std::unique_ptr<Mirror>> mirrors;
    std::map<std::string, std::vector<Mirror*>> mirror_map;

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
