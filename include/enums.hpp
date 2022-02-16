#pragma once

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

    struct Checksum
    {
        ChecksumType type;
        std::string checksum;
    };
}
