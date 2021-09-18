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

enum class DownloadState {
    WAITING, /*!<
        The target is waiting to be processed. */
    PREPARATION,
    RUNNING, /*!<
        The transfer is running. */
    FINISHED, /*!<
        The transfer is successfully finished. */
    FAILED, /*!<
        The transfer is finished without success. */
} ;

enum class HeaderCbState {
    DEFAULT, /*!<
        Default state */
    HTTP_STATE_OK, /*!<
        HTTP headers with OK state */
    INTERRUPTED, /*!<
        Download was interrupted (e.g. Content-Length doesn't match
        expected size etc.) */
    DONE, /*!<
        All headers which we were looking for are already found*/
} ;

/** Enum with zchunk file status */
enum class ZckState {
    HEADER_CK, /*!<
        The zchunk file is waiting to check whether the header is available
        locally. */
    HEADER, /*!<
        The zchunk file is waiting to download the header */
    BODY_CK, /*!<
        The zchunk file is waiting to check what chunks are available locally */
    BODY, /*!<
        The zchunk file is waiting for its body to be downloaded. */
    FINISHED /*!<
        The zchunk file is finished being downloaded. */
} ;

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
    SHA256
};

struct Checksum
{
    ChecksumType type;
    // std::vector<char> checksum;
    std::string checksum;
};

/** Called when a transfer is done (use transfer status to check
 * if successful or failed).
 * @param clientp           Pointer to user data.
 * @param status            Transfer status
 * @param msg               Error message or NULL.
 * @return                  See LrCbReturnCode codes
 */
typedef CbReturnCode (*EndCb)(TransferStatus status,
                       const std::string& msg,
                       void *clientp);
