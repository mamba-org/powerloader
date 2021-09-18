#include "mirror.hpp"
#include "mirrors/oci.hpp"
#include "target.hpp"

bool Mirror::prepare(Target *target)
{
    state = MirrorState::READY;
    return true;
}

bool Mirror::need_preparation(Target *target)
{
    return false;
}

std::string Mirror::format_url(Target* target)
{
    return fmt::format("{}/{}", mirror.url, target->target->path);
}