#pragma once

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>

extern "C"
{
#ifndef _WIN32
#include <unistd.h>
#endif
#include <fcntl.h>
}

namespace fs = std::filesystem;

#include "context.hpp"
#include "curl.hpp"
#include "download_target.hpp"
#include "enums.hpp"
#include "mirror.hpp"
#include "target.hpp"
#include "utils.hpp"
#ifdef WITH_ZCHUNK
#include <zck.hpp>
#endif
#include "result.hpp"

namespace powerloader
{
    struct XError
    {
        enum
        {
            INFO,
            SERIOUS,
            FATAL
        } level;
        std::string reason;
    };

    class Downloader
    {
    public:
        Downloader();
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
        bool check_finished_transfer_status(CURLMsg* msg, Target* target);

        bool is_max_mirrors_unlimited();

        std::shared_ptr<Mirror> select_suitable_mirror(Target* target);

        cpp::result<std::pair<Target*, std::string>, XError> select_next_target();

        bool prepare_next_transfer(bool* candidate_found);

        bool prepare_next_transfers();

        /**
         * @brief Returns whether the download can be retried, using the same URL in
         * case of base_url or full path, or using another mirror in case of using
         * mirrors.
         *
         * @param complete_path_or_base_url determine type of download - mirrors or
         * base_url/fullpath
         * @return gboolean Return TRUE when another chance to download is allowed.
         */
        bool can_retry_download(int num_of_tried_mirrors, const std::string& url);
        bool check_msgs(bool failfast);
        bool set_max_speeds_to_transfers();
        bool download();

        bool failfast = false;
        CURLM* multi_handle;

        std::vector<Target*> m_targets;
        std::vector<Target*> m_running_transfers;

        int allowed_mirror_failures = 10;
        int max_mirrors_to_try = -1;
        int max_connection_per_host = -1;
        std::size_t max_parallel_connections = 5;

        std::map<std::string, std::vector<std::shared_ptr<Mirror>>> mirror_map;
    };

}
