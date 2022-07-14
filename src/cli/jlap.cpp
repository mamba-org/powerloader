#include <powerloader/curl.hpp>
#include <powerloader/context.hpp>
#include <powerloader/fileio.hpp>

using namespace powerloader;
namespace fs = std::filesystem;

struct Patch
{
	std::string hash;
	std::vector<nlohmann::json> patches;
};

void apply_jlap()
{
	std::ifstream jlap_file("repodata.jlap");
	std::ifstream json_file("repodata.json");
	std::string line;
	
	std::vector<Patch> patches;
	std::unique_ptr<Patch> cur_patch;
	while (std::getline(jlap_file, line))
	{
		if (line.size() && line[0] != '{')
		{
			if (cur_patch) patches.push_back(*cur_patch);

			cur_patch = std::make_unique<Patch>();
			cur_patch->hash = line;		
		}
		else
		{
			auto j = nlohmann::json::parse(line);
			std::cout << j.dump(4) << std::endl;
			cur_patch->patches.emplace_back(std::move(j));
		}
	}
}


int main()
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

	outfile.write(response.content.value());
	apply_jlap();
}