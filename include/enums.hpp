#pragma once

#define CACHEDIR "/Users/wolfvollprecht/Programs/powerdownloader/cache"

enum class Protocol
{
    OTHER,
    FILE,
    HTTP,
    // Want: S3, OCI
    // FTP,
    // RSYNC,
};

enum class DownloadState
{
    // The target is waiting to be processed.
    WAITING,
    // The target (or mirror) is running preparation requests (e.g. for auth)
    PREPARATION,
    // The transfer is running.
    RUNNING,
    // The transfer is successfully finished.
    FINISHED,
    // The transfer is finished without success.
    FAILED,
};

enum class HeaderCbState
{
    // Default state
    DEFAULT,
    // HTTP headers with OK state
    HTTP_STATE_OK,
    // Download was interrupted (e.g. Content-Length doesn't match
    // expected size etc.)
    INTERRUPTED,
    // All headers which we were looking for are already found
    DONE
};

/** Enum with zchunk file status */
enum class ZckState
{
    // The zchunk file is waiting to check whether the header is available locally.
    HEADER_CK,
    // The zchunk file is waiting to download the header
    HEADER,
    // The zchunk file is waiting to check what chunks are available locally
    BODY_CK,
    // The zchunk file is waiting for its body to be downloaded.
    BODY,
    // The zchunk file is finished being downloaded.
    FINISHED
};

enum class CbReturnCode
{
    OK = 0,
    ABORT,
    ERROR
};

enum class TransferStatus
{
    SUCCESSFUL,
    ALREADYEXISTS,
    ERROR,
};

enum class ChecksumType
{
    SHA1,
    SHA256,
    MD5
};

struct Checksum
{
    ChecksumType type;
    std::string checksum;
};

/** Called when a transfer is done (use transfer status to check
 * if successful or failed).
 * @param clientp           Pointer to user data.
 * @param status            Transfer status
 * @param msg               Error message or NULL.
 * @return                  See LrCbReturnCode codes
 */
typedef CbReturnCode (*EndCb)(TransferStatus status, const std::string& msg, void* clientp);
