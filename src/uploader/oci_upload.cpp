#include <filesystem>
#include <spdlog/fmt/fmt.h>

#include "mirrors/oci.hpp"

template <class S>
struct UploadChunk
{
    std::size_t endpos;
    S* stream;

    std::size_t total = 0;
};

template <class T>
std::size_t
read_chunk_callback(char* ptr, std::size_t size, std::size_t nmemb, T* uchunk)
{
    // TODO stream error handling?!
    // copy as much data as possible into the 'ptr' buffer, but no more than
    // 'size' * 'nmemb' bytes!
    std::size_t total = size * nmemb;
    if (std::size_t(uchunk->stream->tellg()) + total > uchunk->endpos)
        total = uchunk->endpos - uchunk->stream->tellg();
    uchunk->stream->read(ptr, total);
    std::size_t x = uchunk->stream->gcount();

    uchunk->total += x;
    spdlog::info("Uploading chunk {} bytes of requested {} data (Total now {})", uchunk->stream->gcount(), size * nmemb, uchunk->total);

    return x;
}

template <class S>
Response
upload_oci_chunk(CURLHandle& c, S& stream, std::size_t pos, std::size_t chunk_size)
{
    static UploadChunk<S> upload_chunk;
    upload_chunk.stream = &stream;
    upload_chunk.endpos = pos + chunk_size;

    c.setopt(CURLOPT_UPLOAD, 1L);
    // Content-Type: application/octet-stream
    // Content-Range: <range>
    // Content-Length: <length>
    stream.seekg(pos, stream.beg);
    c.add_header(fmt::format("Content-Range: {}-{}", pos, upload_chunk.endpos - 1));
    c.add_header(fmt::format("Content-Length: {}", chunk_size));
    c.add_header("Content-Type: application/octet-stream");

    c.setopt(CURLOPT_VERBOSE, 1L);
    c.setopt(CURLOPT_INFILESIZE_LARGE, chunk_size);
    c.setopt(CURLOPT_CUSTOMREQUEST, "PATCH");
    c.setopt(CURLOPT_READFUNCTION, read_chunk_callback<UploadChunk<S>>);
    c.setopt(CURLOPT_READDATA, &upload_chunk);
    auto r = c.perform();
    return r;
}

namespace detail
{
    std::string format_url(const OCIMirror& mirror, const std::string& location)
    {
        if (!contains(location, "://"))
        {
            return fmt::format("{}{}", mirror.url, location);
        }
        return location;
    }
}


void upload_chunked(const fs::path& file, OCIMirror& mirror, const std::string& reference, const std::string& digest)
{
    std::string preupload_url = mirror.get_preupload_url(reference);
    auto response = CURLHandle(preupload_url)
                        .setopt(CURLOPT_CUSTOMREQUEST, "POST")
                        .add_headers(mirror.get_auth_headers(reference))
                        .add_header("Content-Length: 0")
                        .perform();

    assert(response.http_status == 202);

    std::string upload_url = detail::format_url(mirror, response.header["location"]);

    std::ifstream ufile(file, std::ios::in | std::ios::binary);

    ufile.seekg(0, ufile.end);
    std::size_t file_size = ufile.tellg();
    ufile.seekg(0, ufile.beg);
    std::size_t chunk_size = 500'000;
    std::size_t pos = 0;

    while (true)
    {
        CURLHandle chandle(upload_url);

        if (pos + chunk_size > file_size)
        {
            chunk_size = file_size - pos;
        }
        chandle.reset_headers();
        chandle.add_headers(mirror.get_auth_headers(reference));

        std::cout << "Uploading chunk " << pos << std::endl;
        auto response = upload_oci_chunk(chandle, ufile, pos, chunk_size);

        upload_url = detail::format_url(mirror, response.header["location"]);

        pos += chunk_size;
        if (pos >= file_size)
            break;

        assert(response.http_status == 202);
    }

    std::string upload_url_digest;
    if (contains(upload_url, "?"))
    {
        upload_url_digest = fmt::format("{}&digest={}", upload_url, digest);
    }
    else
    {
        upload_url_digest = fmt::format("{}?digest={}", upload_url, digest);
    }


    CURLHandle finalize;
    finalize.url(upload_url_digest);
    finalize.setopt(CURLOPT_CUSTOMREQUEST, "PUT");
    finalize.setopt(CURLOPT_VERBOSE, 1);
    finalize.add_headers(mirror.get_auth_headers(reference));
    finalize.add_header("Content-Type: application/octet-stream");
    auto res = finalize.perform();
    std::cout << res.content.str() << std::endl;
}

auto upload_monolithically(const fs::path& file, OCIMirror& mirror, const std::string& reference, const std::string& digest, std::size_t fsize)
{
    std::string preupload_url = mirror.get_preupload_url(reference);
    auto response = CURLHandle(preupload_url)
                        .setopt(CURLOPT_CUSTOMREQUEST, "POST")
                        .add_headers(mirror.get_auth_headers(reference))
                        .perform();

    assert(response.http_status == 202);
    std::string upload_url = detail::format_url(mirror, response.header["location"]) + fmt::format("?digest={}", digest);

    spdlog::info("Upload url: {}", upload_url);

    CURLHandle chandle(upload_url);

    std::ifstream ufile(file, std::ios::in | std::ios::binary);
    chandle.setopt(CURLOPT_UPLOAD, 1L)
        .add_headers(mirror.get_auth_headers(reference))
        .add_header("Content-Type: application/octet-stream")
        .upload(ufile);

    auto cres = chandle.perform();
    assert(cres.http_status == 201);
    return cres;
}


Response
oci_upload(OCIMirror& mirror,
           const std::string& reference,
           const std::string& tag,
           const fs::path& file, 
           bool chunked)
{
    std::string digest = fmt::format("sha256:{}", sha256sum(file));
    std::size_t fsize = fs::file_size(file);

    CURLHandle auth_handle;
    if (mirror.prepare(reference, auth_handle))
    {
        auth_handle.perform();
    }
    std::cout << "Uploading " << file << std::endl;
    if (chunked)
    {
        std::cout << "Uploading chunked" << std::endl;
        upload_chunked(file, mirror, reference, digest);
    }
    else
    {
        std::cout << "Uploading monolithically" << std::endl;
        upload_monolithically(file, mirror, reference, digest, fsize);
    }
    
    // CURLHandle check_empty_manifest_exists()

    // Now we need to upload the manifest for OCI servers
    std::string manifest_url = fmt::format("{}/v2/{}/manifests/{}", mirror.url, reference, tag);
    std::string manifest = mirror.create_manifest(fsize, digest);

    std::istringstream manifest_stream(manifest);

    CURLHandle mhandle(manifest_url);
    mhandle.add_headers(mirror.get_auth_headers(reference))
        .add_header("Content-Type: application/vnd.oci.image.manifest.v1+json")
        .upload(manifest_stream);

    return mhandle.perform();
}
