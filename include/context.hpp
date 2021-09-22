#pragma once

#include <vector>
#include <string>

class Context
{
public:
    bool offline = false;
    int verbosity = 0;

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
