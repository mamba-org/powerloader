#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

size_t
read_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    FILE* readhere = (FILE*) userdata;
    curl_off_t nread;

    // copy as much data as possible into the 'ptr' buffer, but no more than
    // 'size' * 'nmemb' bytes!
    size_t retcode = fread(ptr, size, nmemb, readhere);

    nread = (curl_off_t) retcode;

    fprintf(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T " bytes from file\n", nread);
    return retcode;
}

void
add_multipart_upload(CURL* target,
                     const std::vector<std::string>& files,
                     const std::map<std::string, std::string>& extra_fields)
{
    curl_mimepart* part;
    curl_mime* mime;
    mime = curl_mime_init(target);

    for (const auto& f : files)
    {
        part = curl_mime_addpart(mime);
        curl_mime_filedata(part, f.c_str());
        curl_mime_name(part, "files");
    }

    for (const auto& [k, v] : extra_fields)
    {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, k.c_str());
        curl_mime_data(part, v.c_str(), CURL_ZERO_TERMINATED);
    }

    curl_easy_setopt(target, CURLOPT_MIMEPOST, mime);
}

int
main(void)
{
    std::string url("http://localhost:8000/api/channels/channel0/files");

    std::vector<std::string> files{ "xtensor-0.23.10-hc021e02_0.tar.bz2",
                                    "adlfs-0.7.5-pyhd8ed1ab_0.tar.bz2" };
    std::cout << "Uploading a package ... to " << url << std::endl;
    CURL* curl;
    CURLcode res;
    struct stat file_info;
    curl_off_t speed_upload, total_time;
    FILE* fd;

    // fd = fopen(xtensor, "rb"); /* open file to upload */
    // if (!fd)
    //     return 1; /* can't continue */

    /* to get the file size */
    // if (fstat(fileno(fd), &file_info) != 0)
    //     return 1; /* can't continue */

    curl = curl_easy_init();
    if (curl)
    {
        /* upload to this place */
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        curl_slist* headers;
        headers = curl_slist_append(headers, "X-API-Key: b968980f02c749c0840045c3c3ea8b1d");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        /* tell it to "upload" to the URL */
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        // curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        add_multipart_upload(curl, files, { { "force", "true" } });

        // curl_mime_filename(part, "image.png");

        // curl_mime_data(part, "This is the field data", CURL_ZERO_TERMINATED);

        // /* set where to read from (on Windows you need to use READFUNCTION too)
        // */ curl_easy_setopt(curl, CURLOPT_READDATA, fd);

        // /* set callback to use */
        // curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

        // /* and give the size of the upload (optional) */
        // curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
        //                  (curl_off_t)file_info.st_size);

        /* enable verbose for easier tracing */
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        else
        {
            /* now extract transfer info */
            curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD_T, &speed_upload);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &total_time);

            fprintf(stderr,
                    "Speed: %" CURL_FORMAT_CURL_OFF_T " bytes/sec during %" CURL_FORMAT_CURL_OFF_T
                    ".%06ld seconds\n",
                    speed_upload,
                    (total_time / 1000000),
                    (long) (total_time % 1000000));
        }
        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    fclose(fd);
    return 0;
}
