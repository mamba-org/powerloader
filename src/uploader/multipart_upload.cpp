#include <powerloader/curl.hpp>

void
add_multipart_upload(CURLHandle& target,
                     const std::vector<std::string>& files,
                     const std::map<std::string, std::string>& extra_fields)
{
    curl_mimepart* part;
    curl_mime* mime;
    mime = curl_mime_init(target.handle())

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
