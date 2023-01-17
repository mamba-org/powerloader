#include "compression.hpp"

namespace powerloader
{
    size_t ZstdStream::write(char* in, size_t size)
    {
        ZSTD_inBuffer input = { in, size, 0 };
        ZSTD_outBuffer output = { buffer, BUFFER_SIZE, 0 };

        while (input.pos < input.size)
        {
            auto ret = ZSTD_decompressStream(stream, &output, &input);
            if (ZSTD_isError(ret))
            {
                spdlog::error("ZSTD decompression error: {}", ZSTD_getErrorName(ret));
                return size + 1;
            }
            if (output.pos > 0)
            {
                size_t wcb_res = m_write_callback(buffer, 1, output.pos, m_write_callback_data);
                if (wcb_res != output.pos)
                {
                    return size + 1;
                }
                output.pos = 0;
            }
        }
        return size;
    }
}
