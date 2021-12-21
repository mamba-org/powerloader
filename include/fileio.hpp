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
        fs::path m_path;

    public:
#ifdef _WIN32
        constexpr static wchar_t append_update_binary[] = L"ab+";
        constexpr static wchar_t read_update_binary[] = L"rb+";
        constexpr static wchar_t write_update_binary[] = L"wb+";
        constexpr static wchar_t read_binary[] = L"rb";
#else
        constexpr static char append_update_binary[] = "ab+";
        constexpr static char read_update_binary[] = "rb+";
        constexpr static char write_update_binary[] = "wb+";
        constexpr static char read_binary[] = "rb";
#endif

        FileIO() = default;

#ifdef _WIN32
        inline explicit FileIO(const fs::path& file_path,
                               const wchar_t* mode,
                               std::error_code& ec) noexcept
            : m_path(file_path)
        {
            m_fs = ::_wfsopen(file_path.wstring().c_str(), mode, _SH_DENYNO);
            if (!m_fs)
            {
                ec.assign(GetLastError(), std::generic_category());
                spdlog::error("Could not open file: {}", ec.message());
            }
        }
#else
        inline explicit FileIO(const fs::path& file_path,
                               const char* mode,
                               std::error_code& ec) noexcept
            : m_path(file_path)
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
#ifndef _WIN32
            return ::fileno(m_fs);
#else
            return ::_fileno(m_fs);
#endif
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
#ifdef _WIN32
            return fs::resize_file(m_path, length, ec);
#else
            ec.clear();
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
            if (!m_fs)
            {
                ec.clear();
                return;
            }
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

        inline int error()
        {
            return ::ferror(m_fs);
        }

        void replace_contents_with(const FileIO& other, std::error_code& ec)
        {
            const int bufsize = 32'768;
            char buf[bufsize];
            size_t size;

            other.seek(0, SEEK_SET);
            seek(0, SEEK_SET);

            while ((size = other.read(buf, 1, bufsize)) > 0)
            {
                if (this->write(buf, 1, size) == size)
                {
                    ec.assign(error(), std::generic_category());
                }
            }
            if (ec)
                return;

            truncate(other.seek(0, SEEK_END), ec);
            seek(0, SEEK_SET);
        }
    };
}
