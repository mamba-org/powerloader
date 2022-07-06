#ifndef POWERLOADER_DOWNLOADER_HPP
#define POWERLOADER_DOWNLOADER_HPP

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>

#include <tl/expected.hpp>

extern "C"
{
#ifndef _WIN32
#include <unistd.h>
#endif
#include <fcntl.h>
}

#include <powerloader/export.hpp>
#include <powerloader/context.hpp>
#include <powerloader/curl.hpp>
#include <powerloader/download_target.hpp>
#include <powerloader/enums.hpp>
#include <powerloader/mirror.hpp>
#include <powerloader/target.hpp>
#include <powerloader/utils.hpp>
#include <powerloader/errors.hpp>

namespace powerloader
{
    namespace fs = std::filesystem;

    class Context;

    class POWERLOADER_API Downloader
    {
    public:
        explicit Downloader(const Context& ctx);
        ~Downloader();

        void add(const std::shared_ptr<DownloadTarget>& dl_target);

        /** Check the finished transfer
         * Evaluate CURL return code and status code of protocol if needed.
         * @param serious_error     Serious error is an error that isn't fatal,
         *                          but mirror that generate it should be penalized.
         *                          E.g.: Connection timeout - a mirror we are unable
         *                          to connect at is pretty useless for us, but
         *                          this could be only temporary state.
         *                          No fatal but also no good.
         * @param fatal_error       An error that cannot be recovered - e.g.
         *                          we cannot write to a socket, we cannot write
         *                          data to disk, bad function argument, ...
         */
        tl::expected<void, DownloaderError> check_finished_transfer_status(CURLMsg* msg,
                                                                           Target* target);

        bool is_max_mirrors_unlimited();

        tl::expected<std::shared_ptr<Mirror>, DownloaderError> select_suitable_mirror(
            Target* target);

        tl::expected<std::pair<Target*, std::string>, DownloaderError> select_next_target();

        bool prepare_next_transfer(bool* candidate_found);

        bool prepare_next_transfers();

        /**
         * @brief Returns whether the download can be retried, using the same URL in
         * case of base_url or full path, or using another mirror in case of using
         * mirrors.
         *
         * @param complete_path_or_base_url determine type of download - mirrors or
         * base_url/fullpath
         * @return Return true when another chance to download is allowed.
         */
        bool can_retry_download(int num_of_tried_mirrors, const std::string& url);
        bool check_msgs(bool failfast);
        bool set_max_speeds_to_transfers();
        bool download();

        bool failfast = false;
        CURLM* multi_handle;
        const Context& ctx;

        std::vector<Target*> m_targets;
        std::vector<Target*> m_running_transfers;

        int allowed_mirror_failures = 3;
        int max_mirrors_to_try = -1;
        std::size_t max_parallel_connections = 5;
    };

}

#endif
