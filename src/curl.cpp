#include "curl.hpp"
#include "utils.hpp"

#include <cassert>
#include <fstream>
#include <sstream>

template <class T>
std::size_t
read_callback(char* ptr, std::size_t size, std::size_t nmemb, T* stream)
{
    // TODO stream error handling?!
    // copy as much data as possible into the 'ptr' buffer, but no more than
    // 'size' * 'nmemb' bytes!
    stream->read(ptr, size * nmemb);
    pfdebug("Uploading {} bytes of data!", stream->gcount());
    return stream->gcount();
}

template std::size_t
read_callback<std::ifstream>(char* ptr, std::size_t size, std::size_t nmemb, std::ifstream* stream);
template std::size_t
read_callback<std::istringstream>(char* ptr,
                                  std::size_t size,
                                  std::size_t nmemb,
                                  std::istringstream* stream);
