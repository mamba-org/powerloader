#ifndef POWERLOADER_COMPRESSION_HPP
#define POWERLOADER_COMPRESSION_HPP

#ifdef WITH_ZSTD

#include <spdlog/spdlog.h>

namespace powerloader
{
#include <zstd.h>
#include <curl/curl.h>

    template <class T>
    using curl_write_callback_t = size_t (*)(char* ptr, size_t size, size_t nmemb, T* self);

    class ZstdStream
    {
    public:
        constexpr static size_t BUFFER_SIZE = 256000;

        template <class T>
        ZstdStream(curl_write_callback_t<T> write_callback, T* write_callback_data)
            : stream(ZSTD_createDCtx())
            , m_write_callback((curl_write_callback) write_callback)
            , m_write_callback_data(write_callback_data)
        {
            ZSTD_initDStream(stream);
        }

        ~ZstdStream()
        {
            ZSTD_freeDCtx(stream);
        }

        size_t write(char* in, size_t size);

        static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* self)
        {
            return static_cast<ZstdStream*>(self)->write(ptr, size * nmemb);
        }

    private:
        ZSTD_DCtx* stream;
        char buffer[BUFFER_SIZE];

        // original curl callback
        curl_write_callback m_write_callback;
        void* m_write_callback_data;
    };
}

#endif

#endif  // POWERLOADER_COMPRESSION_HPP
