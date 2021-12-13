#include <filesystem>
#include <spdlog/spdlog.h>

#include <limits.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#else
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace powerloader
{
    class FileIO
    {
    private:
        FILE* m_fs = nullptr;

    public:
        FileIO() = default;

#ifdef _WIN32
        inline explicit FileIO(const fs::path& file_path,
                               const wchar_t* mode,
                               std::error_code& ec) noexcept
        {
            ec.assign(::_wfopen_s(&m_fs, file_path.wstring().c_str(), mode),
                      std::generic_category());
        }
#else
        inline explicit FileIO(const fs::path& file_path,
                               const char* mode,
                               std::error_code& ec) noexcept
        {
            m_fs = ::fopen(file_path.c_str(), mode);
            if (m_fs)
            {
                ec.clear();
            }
            else
            {
                ec.assign(errno, std::generic_category());
                spdlog::error("Could not open file: {}", ec.message());
            }
        }
#endif

        inline ~FileIO()
        {
            if (m_fs)
            {
                std::error_code ec;
                close(ec);
                if (ec)
                {
                    spdlog::error("Error: {}", ec.message());
                }
            }
        }

        inline int fd() const noexcept
        {
            return ::fileno(m_fs);
        }

        inline int seek(int offset, int origin) const noexcept
        {
            return ::fseek(m_fs, offset, origin);
        }

        inline int seek(unsigned int offset, int origin) const noexcept
        {
            return this->seek(static_cast<long long>(offset), origin);
        }

        inline int seek(long offset, int origin) const noexcept
        {
            return ::fseek(m_fs, offset, origin);
        }

        inline int seek(unsigned long offset, int origin) const noexcept
        {
#ifdef _WIN32
            return ::_fseeki64(m_fs, static_cast<long long>(offset), origin);
#else
            assert(offset < LLONG_MAX);
            return ::fseek(m_fs, offset, origin);
#endif
        }

        inline int seek(long long offset, int origin) const noexcept
        {
#ifdef _WIN32
            return ::_fseeki64(m_fs, offset, origin);
#else
            return ::fseek(m_fs, offset, origin);
#endif
        }

        inline bool open()
        {
            return m_fs != nullptr;
        }

        inline long int tell()
        {
            return ::ftell(m_fs);
        }

        inline int seek(unsigned long long offset, int origin) const noexcept
        {
            assert(offset < LLONG_MAX);
            return this->seek(static_cast<long long>(offset), origin);
        }

        inline int eof() const noexcept
        {
            return ::feof(m_fs);
        }

        inline std::size_t read(void* buffer,
                                std::size_t element_size,
                                std::size_t element_count) const noexcept
        {
            assert(m_fs);
            return ::fread(buffer, element_size, element_count, m_fs);
        }

        inline std::size_t write(const void* buffer,
                                 std::size_t element_size,
                                 std::size_t element_count) const noexcept
        {
            return ::fwrite(buffer, element_size, element_count, m_fs);
        }

        inline int put(int c) const noexcept
        {
            return ::fputc(c, m_fs);
        }

        void truncate(off_t length, std::error_code& ec) const noexcept
        {
            ec.clear();
#ifdef _WIN32
            if (SetFilePointerEx(m_fs, length, nullptr, FILE_BEGIN) == 0 || SetEndOfFile(m_fs) == 0)
            {
                ec.assign(static_cast<int>(::GetLastError()), std::system_category());
            }
#else
            if (::ftruncate(fd(), length) != 0)
            {
                ec.assign(errno, std::generic_category());
            }
#endif
        }

        inline void flush()
        {
            ::fflush(m_fs);
        }

        void close(std::error_code& ec) noexcept
        {
            if (::fclose(m_fs) == 0)
            {
                ec.clear();
                m_fs = nullptr;
            }
            else
            {
                ec.assign(errno, std::generic_category());
            }
        }
    };
}
