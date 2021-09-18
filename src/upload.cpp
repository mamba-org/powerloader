#include <string>
#include <vector>
#include <map>
#include <iostream>

#include <stdio.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <nlohmann/json.hpp>

#include "mirrors/oci.hpp"
#include "utils.hpp"

// static size_t manifest_upload(char *ptr, size_t size, size_t nmemb, ManifestReadData *userdata)
// {
//     if (userdata->read_pos == 0)
//         return 0;

//     std::size_t to_read = (size * nmemb) > userdata->manifest.size() ? userdata->manifest.size() : size * nmemb;

//     userdata->read_pos -= to_read;
//     memcpy(ptr, userdata->manifest.c_str(), to_read);
//     fprintf(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T " bytes from file\n", to_read);
//     return to_read;
// }

void add_multipart_upload(CURL *target, const std::vector<std::string> &files, const std::map<std::string, std::string> &extra_fields)
{
    curl_mimepart *part;
    curl_mime *mime;
    mime = curl_mime_init(target);

    for (const auto &f : files)
    {
        part = curl_mime_addpart(mime);
        curl_mime_filedata(part, f.c_str());
        curl_mime_name(part, "files");
    }

    for (const auto &[k, v] : extra_fields)
    {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, k.c_str());
        curl_mime_data(part, v.c_str(), CURL_ZERO_TERMINATED);
    }

    curl_easy_setopt(target, CURLOPT_MIMEPOST, mime);
}

static std::size_t header_callback(char *buffer, std::size_t size, std::size_t nitems, std::string *str)
{
    std::string received_chunk(buffer, size * nitems);
    if (starts_with(to_lower(received_chunk), "location: "))
    {
        *str += received_chunk.substr(sizeof("location: "));
        *str = str->substr(0, str->size() - 2);
    }

    std::cout << str << std::endl;
    return size * nitems;
}

int main(void)
{

    std::string auth_bearer = getenv("OCI_AUTH_BEARER");
    std::string aheader = fmt::format("Authentication: Bearer {}", auth_bearer);

    OCIMirror mirror("https://ghcr.io");

    CURLHandle auth_handle;

    if (mirror.prepare("wolfv/artifact", auth_handle))
    {
        auth_handle.perform();
    }
    return 0;

    std::vector<std::string> files{
        "xtensor-0.23.10-hc021e02_0.tar.bz2",
        "adlfs-0.7.5-pyhd8ed1ab_0.tar.bz2"};

    CURL *curl;
    CURLcode res;
    struct stat file_info;
    curl_off_t speed_upload, total_time;
    FILE *fd;

    std::string sha("sha256:c21c3cea6517c2f968548b82008a8f418a5d9f47a41ce1cb796574b5f1bdbb67");

    Response resx = CURLHandle("https://ghcr.io/v2/wolfv/xtensor/blobs/uploads/")
        .setopt(CURLOPT_CUSTOMREQUEST, "POST")
        .add_header(aheader)
        .perform();

    std::string upload_loc = resx.header["location"];
    std::string upload_url = fmt::format("{}{}?digest={}", mirror.mirror.url, upload_loc, sha);

    std::size_t fsize = fs::file_size(files[0]);

    CURLHandle chandle(upload_url);
    chandle
        .setopt(CURLOPT_UPLOAD, 1L)
        .setopt(CURLOPT_VERBOSE, 1L)
        .setopt(CURLOPT_INFILESIZE_LARGE, fsize)
        .add_header(aheader)
        .add_header("Content-Type: application/octet-stream");

    std::ifstream ufile("xtensor-0.23.10-hc021e02_0.tar.bz2", std::ios::in | std::ios::binary);
    set_file_upload_callback(chandle, &ufile);

    // res = curl_easy_perform(c2);
    auto rx = chandle.perform();
    std::cout << rx.http_status << std::endl;
    std::cout << rx.content.str() << std::endl;

    exit(0);

    curl = curl_easy_init();
    if (curl)
    {

        /* upload to this place */
        // curl_easy_setopt(curl, CURLOPT_URL, oci.c_str());

        // curl_slist *headers;
        // headers = curl_slist_append(headers, "X-API-Key: b968980f02c749c0840045c3c3ea8b1d");
        // curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        

        /* tell it to "upload" to the URL */
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        // curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        // add_multipart_upload(curl, files, {{"force", "true"}});

        // https://ghcr.io/token?scope=repository:{}:{}
        // https://ghcr.io/token?scope=repository:push,pull:wolfv/xtensor

        // OCI test
        std::string header_string;
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);

        curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, aheader.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        std::cout << mirror.create_manifest(file_info.st_size, sha);
        // curl_mime_filename(part, "image.png");

        // curl_mime_data(part, "This is the field data", CURL_ZERO_TERMINATED);

        // /* set where to read from (on Windows you need to use READFUNCTION too) */
        // curl_easy_setopt(curl, CURLOPT_READDATA, fd);

        // /* set callback to use */
        // curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

        // /* and give the size of the upload (optional) */
        // curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
        //                  (curl_off_t)file_info.st_size);

        /* enable verbose for easier tracing */
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        res = curl_easy_perform(curl);

        CURL* c2 = curl_easy_init();

        std::string temp_url = std::string("https://ghcr.io/") + header_string + "?digest=" + sha;
        std::cout << "FULL URL: " << temp_url << std::endl;
        curl_easy_setopt(c2, CURLOPT_URL, temp_url.c_str());
        std::cout << "DONE, got headers: " << header_string << std::endl;
        /* Check for errors */

        curl_easy_setopt(c2, CURLOPT_UPLOAD, 1L);

        curl_slist *h2 = nullptr;
        h2 = curl_slist_append(h2, aheader.c_str());
        h2 = curl_slist_append(h2, "Content-Type: application/octet-stream");
        curl_easy_setopt(c2, CURLOPT_HTTPHEADER, h2);

        curl_easy_setopt(c2, CURLOPT_VERBOSE, 1L);

        char xxx[1000];
        std::ifstream ufile("xtensor-0.23.10-hc021e02_0.tar.bz2", std::ios::in | std::ios::binary);
        if (!ufile) {
            std::cout << "ERROR ERROR ERROR!" << std::endl;
            exit(1);
        }
        set_file_upload_callback(c2, &ufile);
    
        /* and give the size of the upload (optional) */
        curl_easy_setopt(c2, CURLOPT_INFILESIZE_LARGE, fsize);

        res = curl_easy_perform(c2);

        CURL* c3 = curl_easy_init();
        curl_slist *h3 = nullptr;
        h3 = curl_slist_append(h3, aheader.c_str());
        h3 = curl_slist_append(h3, "Content-Type: application/vnd.oci.image.manifest.v1+json");
        curl_easy_setopt(c3, CURLOPT_HTTPHEADER, h3);

        std::string manifest_url = "https://ghcr.io/v2/wolfv/xtensor/manifests/1.0";
        std::string manifest = mirror.create_manifest(file_info.st_size, sha);
        curl_easy_setopt(c3, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(c3, CURLOPT_URL, manifest_url.c_str());

        curl_easy_setopt(c3, CURLOPT_UPLOAD, 1L);

        std::istringstream manifest_stream(manifest);
        set_string_upload_callback(c3, &manifest_stream);
        // curl_easy_setopt(c3, CURLOPT_READFUNCTION, manifest_upload);
        // curl_easy_setopt(c3, CURLOPT_READDATA, &md);

        // curl_easy_setopt(c3, CURLOPT_POSTFIELDS, manifest.c_str());
        /* set where to read from (on Windows you need to use READFUNCTION too) */
        // curl_easy_setopt(c3, CURLOPT_READDATA, fd);
    
        /* and give the size of the upload (optional) */
        // curl_easy_setopt(c3, CURLOPT_INFILESIZE_LARGE,
                        // (curl_off_t)file_info.st_size);

        res = curl_easy_perform(c3);


        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
        }
        else
        {
            /* now extract transfer info */
            curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD_T, &speed_upload);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &total_time);

            fprintf(stderr, "Speed: %" CURL_FORMAT_CURL_OFF_T " bytes/sec during %" CURL_FORMAT_CURL_OFF_T ".%06ld seconds\n",
                    speed_upload,
                    (total_time / 1000000), (long)(total_time % 1000000));
        }
        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    return 0;
}