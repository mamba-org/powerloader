#ifndef POWERLOADER_ERRORS_HPP
#define POWERLOADER_ERRORS_HPP

#include <spdlog/spdlog.h>
#include <powerloader/enums.hpp>

namespace powerloader
{
    struct DownloaderError
    {
        ErrorLevel level;
        ErrorCode code;
        std::string reason;

        bool is_serious() const noexcept
        {
            return (level == ErrorLevel::SERIOUS || level == ErrorLevel::FATAL);
        }

        bool is_fatal() const noexcept
        {
            return level == ErrorLevel::FATAL;
        }

        void log() const
        {
            switch (level)
            {
                case ErrorLevel::FATAL:
                    spdlog::critical(reason);
                    break;
                case ErrorLevel::SERIOUS:
                    spdlog::error(reason);
                    break;
                default:
                    spdlog::warn(reason);
            }
        }
    };
}

#endif
