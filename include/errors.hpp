#ifndef PL_ERRORS_HPP
#define PL_ERRORS_HPP

#include <spdlog/spdlog.h>
#include "result.hpp"

namespace powerloader
{
    /** Librepo return/error codes
     */
    enum class ErrorCode
    {
        // everything is ok
        PD_OK,
        // bad function argument
        PD_BADFUNCARG,
        // bad argument of the option
        PD_BADOPTARG,
        // library doesn't know the option
        PD_UNKNOWNOPT,
        // cURL doesn't know the option. Too old curl version?
        PD_CURLSETOPT,
        // Result object is not clean
        PD_ADYUSEDRESULT,
        // Result doesn't contain all what is needed
        PD_INCOMPLETERESULT,
        // cannot duplicate curl handle
        PD_CURLDUP,
        // cURL error
        PD_CURL,
        // cURL multi handle error
        PD_CURLM,
        // HTTP or FTP returned status code which do not represent success
        // (file doesn't exists, etc.)
        PD_BADSTATUS,
        // some error that should be temporary and next try could work
        // (HTTP status codes 500, 502-504, operation timeout, ...)
        PD_TEMPORARYERR,
        // URL is not a local address
        PD_NOTLOCAL,
        // cannot create a directory in output dir (ady exists?)
        PD_CANNOTCREATEDIR,
        // input output error
        PD_IO,
        // bad mirrorlist/metalink file (metalink doesn't contain needed
        // file, mirrorlist doesn't contain urls, ..)
        PD_MIRRORS,
        // bad checksum
        PD_BADCHECKSUM,
        // no usable URL found
        PD_NOURL,
        // cannot create tmp directory
        PD_CANNOTCREATETMP,
        // unknown type of checksum is needed for verification
        PD_UNKNOWNCHECKSUM,
        // bad URL specified
        PD_BADURL,
        // Download was interrupted by signal.
        // Only if LRO_INTERRUPTIBLE option is enabled.
        PD_INTERRUPTED,
        // sigaction error
        PD_SIGACTION,
        // File ady exists and checksum is ok.*/
        PD_ADYDOWNLOADED,
        // The download wasn't or cannot be finished.
        PD_UNFINISHED,
        // select() call failed.
        PD_SELECT,
        // OpenSSL library related error.
        PD_OPENSSL,
        // Cannot allocate more memory
        PD_MEMORY,
        // Interrupted by user cb
        PD_CBINTERRUPTED,
        // File operation error (operation not permitted, filename too long, no memory available,
        // bad file descriptor, ...)
        PD_FILE,
        // Zchunk error (error reading zchunk file, ...)
        PD_ZCK,
        // (xx) unknown error - sentinel of error codes enum
        PD_UNKNOWNERROR,
    };

    enum ErrorLevel
    {
        INFO,
        SERIOUS,
        FATAL
    };

    struct DownloaderError
    {
        ErrorLevel level;
        ErrorCode code;
        std::string reason;

        inline bool is_serious()
        {
            return (level == ErrorLevel::SERIOUS || level == ErrorLevel::FATAL);
        }
        inline bool is_fatal()
        {
            return level == ErrorLevel::FATAL;
        }

        inline void log()
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
