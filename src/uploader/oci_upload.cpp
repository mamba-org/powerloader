#include <filesystem>
#include "mirrors/oci.hpp"

struct OCIUpload
{
    OCIMirror mirror;
    std::string upload_url;
    std::string digest;
    std::string auth_token;

    std::string get_final_url()
    {
        return fmt::format("{}/{}?digest={}", mirror.mirror.url, upload_url, digest);
    }
};

void oci_upload(OCIMirror& mirror, 
                const std::string& reference,
                const std::string& tag,
                const fs::path& file)
{
    std::string digest = fmt::format("sha256:{}", sha256sum(file));
    std::size_t fsize = fs::file_size(file);

    CURLHandle auth_handle;
    if (mirror.prepare(reference, auth_handle))
    {
        auth_handle.perform();
    }

    std::string preupload_url = mirror.get_preupload_url(reference);
    std::cout << "PREUPLOAD URL: " << preupload_url << "\n\n\n\n";
    auto response = CURLHandle(preupload_url)
        .setopt(CURLOPT_CUSTOMREQUEST, "POST")
        .setopt(CURLOPT_VERBOSE, 1L)
        .add_headers(mirror.get_auth_headers(reference))
        .perform();

    std::cout << response.content.str() << std::endl;

    std::string temp_upload_location = response.header["location"];
    std::string upload_url = fmt::format("{}{}?digest={}", mirror.mirror.url, temp_upload_location, digest);
    std::cout << "Upload url: " << upload_url << std::endl;

    CURLHandle chandle(upload_url);
    chandle
        .setopt(CURLOPT_UPLOAD, 1L)
        .setopt(CURLOPT_VERBOSE, 1L)
        .setopt(CURLOPT_INFILESIZE_LARGE, fsize)
        .add_headers(mirror.get_auth_headers(reference))
        .add_header("Content-Type: application/octet-stream");

    std::ifstream ufile(file, std::ios::in | std::ios::binary);
    set_file_upload_callback(chandle, &ufile);

    auto cres = chandle.perform();

    std::cout << cres.http_status << std::endl;
    std::cout << cres.content.str() << std::endl;

    // Now we need to upload the manifest for OCI servers
    std::string manifest_url = fmt::format("{}/v2/{}/manifests/{}", mirror.mirror.url, reference, tag);
    std::string manifest = mirror.create_manifest(fsize, digest);

    CURLHandle mhandle(manifest_url);
    mhandle.setopt(CURLOPT_VERBOSE, 1L)
           .setopt(CURLOPT_UPLOAD, 1L)
           .add_headers(mirror.get_auth_headers(reference))
           .add_header("Content-Type: application/vnd.oci.image.manifest.v1+json");

    std::istringstream manifest_stream(manifest);
    set_string_upload_callback(mhandle, &manifest_stream);

    mhandle.perform();
}