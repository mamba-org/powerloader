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

struct Patch
{
    std::string hash;
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

    std::cout << blake2sum("repodata.json") << std::endl;
    std::cout << blake2sum("repodata.jlap") << std::endl;
    std::vector<Patch> patches;
    std::unique_ptr<Patch> cur_patch;

    while (std::getline(jlap_file, line))
    {
        if (line.size() && line[0] != '{')
        {
            if (cur_patch)
                patches.push_back(*cur_patch);

            cur_patch = std::make_unique<Patch>();
            cur_patch->hash = line;
        }
        else
        {
            try
            {
                auto j = nlohmann::json::parse(line);
                // std::cout << j.dump(4) << std::endl;
                cur_patch->patches.emplace_back(std::move(j));
            }
            catch(...)
            {
                std::cout << "Could not parse patch " << line << std::endl;
            }
        }
    }

    nlohmann::json jrdata;
    json_file >> jrdata;

    std::string repo_bsum = blake2sum("repodata.json");

    std::size_t i = 0;

    for (auto& p : patches)
    {
        std::cout << "Found patches # " << p.patches.size() << std::endl;
        for (auto& pf : p.patches)
        {
            if (pf.contains("from"))
            {
                if (pf["from"] == repo_bsum)
                {
                    auto t0 = std::chrono::high_resolution_clock::now();
                    std::cout << "Applying patch " << i++ << " from " << repo_bsum.substr(0, 8) << "…2 to "
                              << pf["to"].get<std::string>().substr(0, 8) << "… ";
                    repo_bsum = pf["to"];
                    // nlohmann::inplace_patch(jrdata, pf["patch"]);
                    jrdata.patch(pf["patch"]);
                    auto t1 = std::chrono::high_resolution_clock::now();
                    std::cout << "took " << std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count() <<" ns." << std::endl;
                }
            }
        }
    }
    {
        std::ofstream rpatched("final_repodata.json");
        rpatched << jrdata.dump(2) << "\n";
    }

    if (blake2sum("final_repodata.json") == patches.back().patches.back()["latest"])
    {
        std::cout << "All successful." << std::endl;
    }
    else
    {
        std::cout << "Error: " << blake2sum("final_repodata.json") << " not matching " << patches.back().patches.back()["latest"] << std::endl;
    }
}


int
main()
{
    powerloader::Context ctx;
    CURLHandle h(ctx);

    std::error_code ec;
    FileIO outfile("repodata.jlap", FileIO::append_update_binary, ec);
    outfile.seek(0, SEEK_END);
    std::cout << outfile.tell() << std::endl;

    h.url("https://conda.anaconda.org/conda-forge/linux-64/repodata.jlap");
    h.setopt(CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(outfile.tell()));
    auto response = h.perform();
    for (auto& [k, v] : response.headers)
        std::cout << k << " .. " << v << std::endl;

    if (response.ok())
        outfile.write(response.content.value());
    apply_jlap();
    return 0;
}
