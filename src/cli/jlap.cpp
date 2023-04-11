#include <powerloader/curl.hpp>
#include <powerloader/context.hpp>
#include <powerloader/fileio.hpp>
#include <openssl/evp.h>
#include <ctime>

extern "C"
{
#include "blake2.h"
}

using namespace powerloader;
namespace fs = std::filesystem;

struct Jlap
{
    std::string start_hash;
    std::string end_hash;
    nlohmann::json metadata;
    std::vector<nlohmann::json> patches;
};

std::string
blake2sum(const fs::path& path)
{
    size_t sum, n;
    blake2b_state S;
    constexpr std::size_t BUFSIZE = 32768;
    std::vector<char> buffer(BUFSIZE);

    constexpr std::size_t outbytes = 32;
    blake2b_init(&S, outbytes);

    unsigned char hash[32];

    std::ifstream infile(path, std::ios::binary);

    while (infile)
    {
        infile.read(buffer.data(), BUFSIZE);
        size_t count = infile.gcount();
        if (!count)
            break;
        blake2b_update(&S, buffer.data(), count);
    }

    blake2b_final(&S, hash, outbytes);
    return hex_string(hash, outbytes);
}

void
apply_jlap()
{
    std::ifstream jlap_file("repodata.jlap");
    std::ifstream json_file("repodata.json");
    std::string line;

    Jlap jlap_doc;

    std::vector<std::string> lines;
    while (std::getline(jlap_file, line))
    {
        lines.push_back(line);
    }

    if (lines.size() >= 3)
    {
        jlap_doc.start_hash = lines[0];
        jlap_doc.end_hash = lines[lines.size() - 1];

        assert(jlap_doc.start_hash.size() == 64);
        assert(jlap_doc.end_hash.size() == 64);

        try
        {
            jlap_doc.metadata = nlohmann::json::parse(lines[lines.size() - 2]);
            assert(jlap_doc.patches.back().contains("latest"));
            assert(jlap_doc.patches.back().contains("url"));
        }
        catch (...)
        {
            std::cout << "Could not parse metadata " << line << std::endl;
        }

        // parse patches in between
        auto it = lines.begin() + 1;
        for (; it != lines.end() - 2; ++it)
        {
            try
            {
                jlap_doc.patches.push_back(nlohmann::json::parse(*it));
                assert(jlap_doc.patches.back().contains("patch"));
                assert(jlap_doc.patches.back().contains("from"));
                assert(jlap_doc.patches.back().contains("to"));
            }
            catch (...)
            {
                std::cout << "Could not parse patch " << line << std::endl;
            }
        }
    }

    nlohmann::json jrdata;
    json_file >> jrdata;

    std::string repo_bsum = blake2sum("repodata.json");

    std::size_t i = 0;

    std::cout << "Found patches # " << jlap_doc.patches.size() << std::endl;
    for (auto& pf : jlap_doc.patches)
    {
        if (pf["from"] == repo_bsum)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            std::cout << "Applying patch " << i++ << " from " << repo_bsum.substr(0, 8) << "… to "
                      << pf["to"].get<std::string>().substr(0, 8) << "… ";
            repo_bsum = pf["to"];
            jrdata.patch_inplace(pf["patch"]);
            auto t1 = std::chrono::high_resolution_clock::now();
            std::cout << "took "
                      << std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()
                      << " ns." << std::endl;
        }
    }

    {
        std::ofstream rpatched("final_repodata.json");
        rpatched << jrdata.dump(2) << "\n";
    }

    if (blake2sum("final_repodata.json") == jlap_doc.metadata["latest"])
    {
        std::cout << "All successful." << std::endl;
    }
    else
    {
        std::cout << "Error: " << blake2sum("final_repodata.json") << " not matching "
                  << jlap_doc.metadata["latest"] << std::endl;
    }
}


int
main()
{
    powerloader::Context ctx;
    CURLHandle h(ctx);
    std::string filehash;
    std::streamoff total_size = 0, start_offset = 0;

    std::error_code ec;
    if (fs::exists("repodata.jlap"))
    {
        FileIO infile("repodata.jlap", FileIO::read_binary, ec);
        infile.seek(0, SEEK_END);
        total_size = infile.tell();

        char filehashtmp[64] = { 0 };
        if (total_size > 64)
        {
            infile.seek(-64, SEEK_END);
            infile.read(filehashtmp, sizeof(char), 64);
            filehash = std::string(&filehashtmp[0], 64);

            // find line before latest (to cut metadata as well)
            infile.seek(-64 - 2, SEEK_END);
            while (infile.get() != '\n')
            {
                infile.seek(-2, SEEK_CUR);
            }

            start_offset = infile.tell();
        }
    }

    h.url("https://conda.anaconda.org/conda-forge/osx-arm64/repodata.jlap");
    if (start_offset != 0)
    {
        std::cout << "Resuming from offset: " << start_offset << " (cutting "
                  << total_size - start_offset << " bytes)" << std::endl;
        h.setopt(CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(start_offset));
    }
    auto response = h.perform();
    if (!response.ok() && response.http_status == 416) {
        // offset is wrong, overwrite the entire file
        h.setopt(CURLOPT_RESUME_FROM_LARGE, 0);
        response = h.perform();
    }

    if (response.ok())
    {
        auto open_mode = fs::exists("repodata.jlap") ? FileIO::read_update_binary
                                                     : FileIO::write_update_binary;

        FileIO outfile("repodata.jlap", open_mode, ec);
        if (open_mode == FileIO::read_update_binary)
            outfile.seek(start_offset, SEEK_SET);

        std::cout << "Response size: " << response.content.value().size() << std::endl;
        std::string new_filehash = response.content.value().substr(0, 64);
        if (response.content.value().size() == 64)
        {
            std::cout << "making sure that old filehash is equal new file hash: " << new_filehash
                      << " == " << filehash << std::endl;
            assert(new_filehash == filehash);
        }
        outfile.write(response.content.value());
        outfile.flush();
        apply_jlap();
    }
    return 0;
}
