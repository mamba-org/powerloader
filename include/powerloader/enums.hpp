#ifndef POWERLOADER_ENUMS_HPP
#define POWERLOADER_ENUMS_HPP

#define PARTEXT ".pdpart"
#define EMPTY_SHA "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

namespace powerloader
{
    enum class Protocol
    {
        kOTHER,
        kFILE,
        kHTTP,
        kFTP,
        // Want: S3, OCI
    };

    enum CompressionType
    {
        NONE,
        ZSTD,
    };

    enum class MirrorState
    {
        WAITING,
        AUTHENTICATING,
        READY,
        RETRY_DELAY,
        AUTHENTICATION_FAILED,
        FAILED
    };

    enum class DownloadState
    {
        // The target is waiting to be processed.
        kWAITING,
        // The target (or mirror) is running preparation requests (e.g. for auth)
        kPREPARATION,
        // The transfer is running.
        kRUNNING,
        // The transfer is successfully finished.
        kFINISHED,
        // The transfer is finished without success.
        kFAILED,
    };

    enum class HeaderCbState
    {
        // Default state
        kDEFAULT,
        // HTTP headers with OK state
        kHTTP_STATE_OK,
        // Download was interrupted (e.g. Content-Length doesn't match
        // expected size etc.)
        kINTERRUPTED,
        // All headers which we were looking for are already found
        kDONE
    };

    /** Enum with zchunk file status */
    enum class ZckState
    {
        // The zchunk file is waiting to download the header lead if header_size & hash not
        // available
        kHEADER_LEAD,
        // The zchunk file is waiting to check whether the header is available locally.
        kHEADER_CK,
        // The zchunk file is waiting to download the header
        kHEADER,
        // The zchunk file is waiting to check what chunks are available locally
        kBODY_CK,
        // The zchunk file is waiting for its body to be downloaded.
        kBODY,
        // The zchunk file is finished being downloaded.
        kFINISHED
    };

    enum class CbReturnCode
    {
        kOK = 0,
        kABORT,
        kERROR
    };

    enum class TransferStatus
    {
        kSUCCESSFUL,
        kALREADYEXISTS,
        kERROR,
    };

    enum class ChecksumType
    {
        kSHA1,
        kSHA256,
        kMD5
    };

    /** Librepo return/error codes */
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

    struct Checksum
    {
        ChecksumType type;
        std::string checksum;
    };

    /** AS A REFERENCE */
    // typedef enum {
    //   CURLUE_OK,
    //   CURLUE_BAD_HANDLE,          /* 1 */
    //   CURLUE_BAD_PARTPOINTER,     /* 2 */
    //   CURLUE_MALFORMED_INPUT,     /* 3 */
    //   CURLUE_BAD_PORT_NUMBER,     /* 4 */
    //   CURLUE_UNSUPPORTED_SCHEME,  /* 5 */
    //   CURLUE_URLDECODE,           /* 6 */
    //   CURLUE_OUT_OF_MEMORY,       /* 7 */
    //   CURLUE_USER_NOT_ALLOWED,    /* 8 */
    //   CURLUE_UNKNOWN_PART,        /* 9 */
    //   CURLUE_NO_SCHEME,           /* 10 */
    //   CURLUE_NO_USER,             /* 11 */
    //   CURLUE_NO_PASSWORD,         /* 12 */
    //   CURLUE_NO_OPTIONS,          /* 13 */
    //   CURLUE_NO_HOST,             /* 14 */
    //   CURLUE_NO_PORT,             /* 15 */
    //   CURLUE_NO_QUERY,            /* 16 */
    //   CURLUE_NO_FRAGMENT          /* 17 */
    // } CURLUcode;
}

#endif
