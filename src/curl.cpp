#include "curl.hpp"
#include "utils.hpp"

#include <cassert>
#include <fstream>
#include <sstream>

template <class T>
static size_t
read_callback(char* ptr, size_t size, size_t nmemb, T* stream)
{
    // TODO stream error handling?!
    // copy as much data as possible into the 'ptr' buffer, but no more than
    // 'size' * 'nmemb' bytes!
    stream->read(ptr, size * nmemb);
    pfdebug("Uploading {} bytes of data!", stream->gcount());
    return stream->gcount();
}

void
set_string_upload_callback(CURL* handle, std::istringstream* data)
{
    curl_easy_setopt(handle, CURLOPT_READFUNCTION, read_callback<std::istringstream>);
    curl_easy_setopt(handle, CURLOPT_READDATA, data);
}

void
set_file_upload_callback(CURL* handle, std::ifstream* data)
{
    curl_easy_setopt(handle, CURLOPT_READFUNCTION, read_callback<std::ifstream>);
    curl_easy_setopt(handle, CURLOPT_READDATA, data);
}
