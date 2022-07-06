#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <yaml-cpp/yaml.h>

#include <powerloader/mirror.hpp>
#include <powerloader/mirrors/oci.hpp>
#include <powerloader/mirrors/s3.hpp>
#include <powerloader/url.hpp>
#include <powerloader/utils.hpp>
#include <powerloader/downloader.hpp>

#ifdef WITH_ZCHUNK
#include "../zck.hpp"
#endif

using namespace powerloader;

enum KindOf
{
    kHTTP,
    kOCI,
    kS3
};

struct
{
    std::size_t done;
    std::size_t total;

    std::map<DownloadTarget*, curl_off_t> total_done;
} global_progress;

struct MirrorCredentials
{
    URLHandler url;
    std::string user, password, region;
};

static bool show_progress_bars = true;
static bool no_https = false;

int
progress_callback(DownloadTarget* t, curl_off_t total, curl_off_t done)
{
    if (!show_progress_bars)
        return 0;
    if (total == 0 || done == 0)
        return 0;
    if (global_progress.total_done.size() != 0)
        std::cout << "\x1b[1A\r";
    if (global_progress.total_done.find(t) == global_progress.total_done.end())
    {
        if (!total)
            return 0;
        global_progress.total_done[t] = done;
        global_progress.total += total;
    }
    else
    {
        global_progress.total_done[t] = done;
    }

    std::uint64_t total_done = 0;
    for (auto& [k, v] : global_progress.total_done)
        total_done += v;
    total_done /= global_progress.total;

    std::size_t bar_width = 50;
    std::cout << "[";
    std::size_t pos = bar_width * total_done;
    for (std::size_t i = 0; i < bar_width; ++i)
    {
        if (i < pos)
            std::cout << "=";
        else if (i == pos)
            std::cout << ">";
        else
            std::cout << " ";
    }
    std::cout << "] " << total_done * 100 << " %\n";
    std::cout.flush();

    return 0;
}

std::pair<std::string, std::string>
oci_fn_split_tag(const std::string& fn)
{
    // for OCI, if we have a filename like "xtensor-0.23.10-h2acdbc0_0.tar.bz2"
    // we want to split it to `xtensor:0.23.10-h2acdbc0-0`
    std::pair<std::string, std::string> result;
    auto parts = rsplit(fn, "-", 2);

    if (parts.size() < 2)
    {
        spdlog::error("Could not split filename into enough parts");
    }

    result.first = parts[0];

    std::string tag;
    if (parts.size() > 2)
    {
        std::string last_part = parts[2].substr(0, parts[2].find_first_of("."));
        tag = fmt::format("{}-{}", parts[1], last_part);
    }
    else
    {
        tag = parts[1];
    }

    replace_all(tag, "_", "-");
    result.second = tag;

    spdlog::info("Splitting {} to name: {} tag: {}", fn, result.first, result.second);
    return result;
}

int
handle_upload(const Context& ctx,
              const std::vector<std::string>& files,
              const std::vector<std::string>& mirrors)
{
    std::string mirror_url = mirrors[0];
    if (mirrors.size() > 1)
        spdlog::warn("Only uploading to first mirror");

    KindOf kof = KindOf::kHTTP;
    std::unique_ptr<Mirror> mptr;

    URLHandler url(mirror_url);

    if (url.scheme() == "s3")
        kof = KindOf::kS3;
    else if (url.scheme() == "oci")
        kof = KindOf::kOCI;

    if (kof != KindOf::kHTTP)
        url.set_scheme(no_https ? "http" : "https");

    spdlog::info("URL: {}", url.url());

    for (auto& f : files)
    {
        auto elems = split(f, ":");
        fs::path fpath = elems[0];
        std::string dest = elems[1];

        if (kof == KindOf::kOCI)
        {
            if (elems.size() != 3)
            {
                spdlog::error("For OCI upload we need file:destname:tag");
                return 1;
            }
            std::string GH_SECRET = get_env("GHA_PAT", "");
            std::string GH_USER = get_env("GHA_USER", "");

            OCIMirror mirror{ ctx, url.url(), GH_USER, "push", GH_USER, GH_SECRET };
            try
            {
                auto res
                    = oci_upload(ctx,
                                 mirror,
                                 dest,
                                 elems[2],
                                 { OCILayer::from_file("application/octet-stream", elems[0]) });
                if (res.ok())
                {
                    std::cout << "Finished upload for " << f << " to OCI Registry at " << url.url()
                              << std::endl;
                }
                else
                {
                    std::cout << "Could not upload " << f << " to OCI Registry at " << url.url()
                              << std::endl;
                    return 1;
                }
            }
            catch (std::exception& e)
            {
                spdlog::critical("Could not upload: {}", e.what());
                return 1;
            }
        }
        else if (kof == KindOf::kS3)
        {
            if (elems.size() != 2)
            {
                spdlog::error("For S3 upload we need file:destpath");
                return 1;
            }

            std::string aws_ackey = get_env("AWS_ACCESS_KEY_ID");
            std::string aws_sekey = get_env("AWS_SECRET_ACCESS_KEY");
            std::string aws_region = get_env("AWS_DEFAULT_REGION");

            std::string url_ = url.url();
            if (url_.back() == '/')
                url_ = url_.substr(0, url_.size() - 1);

            S3Mirror s3mirror{ ctx, url_, aws_region, aws_ackey, aws_sekey };
            try
            {
                s3_upload(ctx, s3mirror, elems[1], elems[0]);
                std::cout << "Finished upload for " << f << " to S3 bucket at" << url_ << std::endl;
            }
            catch (std::exception& e)
            {
                spdlog::critical("Could not upload: {}", e.what());
                return 1;
            }
        }
    }

    return 0;
}

struct DownloadMetadata
{
    std::string outfile;
    std::string sha256;
    std::ptrdiff_t filesize = -1;

    std::string zck_header_sha256;
    std::ptrdiff_t zck_header_size = -1;
};

int
handle_download(Context& ctx,
                const std::vector<std::string>& urls,
                const std::vector<std::string>& mirrors,
                bool resume,
                const std::string& dest_folder,
                DownloadMetadata& metadata,
                bool do_zck_extract)
{
    // the format for URLs is: <mirror>:<path> (e.g. conda-forge:linux-64/xtensor-123.tar.bz2) or
    // https://conda.anaconda.org/conda-forge/linux-64/xtensor-123.tar.bz2
    std::vector<std::shared_ptr<DownloadTarget>> targets;

    for (auto& x : urls)
    {
        if (contains(x, "://"))
        {
            // even when we get a regular URL like `http://test.com/download.tar.gz`
            // we want to create a "mirror" for `http://test.com` to make sure we correctly
            // retry and wait on mirror failures
            URLHandler uh(x);
            std::string url = uh.url();
            std::string host = uh.host();
            std::string path = uh.path();
            std::string mirror_url = url.substr(0, url.size() - path.size());
            std::string dst
                = metadata.outfile.empty() ? rsplit(uh.path(), "/", 1).back() : metadata.outfile;

            if (ctx.mirror_map.find(host) == ctx.mirror_map.end())
            {
                ctx.mirror_map[host] = std::vector<std::shared_ptr<Mirror>>();
            }
            ctx.mirror_map[host].push_back(std::make_shared<Mirror>(ctx, mirror_url));
            targets.emplace_back(new DownloadTarget(path.substr(1, std::string::npos), host, dst));
        }
        else
        {
            std::vector<std::string> parts = split(x, ":");
            std::string path, mirror;
            if (parts.size() == 2)
            {
                mirror = parts[0];
                path = parts[1];
            }
            else
            {
                throw std::runtime_error("Not the correct number of : in the url");
            }
            std::string dst
                = metadata.outfile.empty() ? rsplit(path, "/", 1).back() : metadata.outfile;

            if (!dest_folder.empty())
                dst = dest_folder + "/" + dst;

            spdlog::info("Downloading {} from {} to {}", path, mirror, dst);
            targets.emplace_back(new DownloadTarget(path, mirror, dst));
        }
        targets.back()->resume = resume;

        if (!metadata.sha256.empty())
            targets.back()->checksums.push_back(Checksum{ ChecksumType::kSHA256, metadata.sha256 });
        if (metadata.filesize > 0)
            targets.back()->expected_size = metadata.filesize;
            // TODO we should have two different fields for those two
#ifdef WITH_ZCHUNK
        if (!metadata.zck_header_sha256.empty())
            targets.back()->p_zck->zck_header_checksum = std::make_unique<Checksum>(
                Checksum{ ChecksumType::kSHA256, metadata.zck_header_sha256 });
        if (metadata.zck_header_size > 0)
            targets.back()->p_zck->zck_header_size = metadata.zck_header_size;
#endif

        using namespace std::placeholders;
        targets.back()->progress_callback
            = std::bind(&progress_callback, targets.back().get(), _1, _2);
    }

    Downloader dl{ ctx };

    for (auto& t : targets)
    {
        dl.add(t);
    }

    bool success = dl.download();
    if (!success)
    {
        spdlog::error("Download was not successful");
        exit(1);
    }

#ifdef WITH_ZCHUNK
    if (do_zck_extract)
    {
        for (auto& t : targets)
        {
            if (t->is_zchunk)
            {
                fs::path p = t->fn;
                fs::path p_ext = p;
                p_ext.replace_extension("");
                zck_extract(p, p_ext, false);
            }
        }
    }
#endif
    return 0;
}

std::map<std::string, std::vector<std::shared_ptr<Mirror>>>
parse_mirrors(const Context& ctx, const YAML::Node& node)
{
    assert(node.IsMap());
    std::map<std::string, std::vector<std::shared_ptr<Mirror>>> res;

    auto get_env_from_str = [](const std::string& s, const std::string default_val)
    {
        if (starts_with(s, "env:"))
        {
            return get_env(s.substr(4).c_str());
        }
        if (!s.empty())
        {
            return get_env(default_val.c_str());
        }
        return s;
    };

    for (YAML::Node::const_iterator oit = node.begin(); oit != node.end(); ++oit)
    {
        std::string mirror_name = oit->first.as<std::string>();
        res[mirror_name] = std::vector<std::shared_ptr<Mirror>>();

        assert(oit->second.IsSequence());
        for (YAML::Node::const_iterator it = oit->second.begin(); it != oit->second.end(); ++it)
        {
            MirrorCredentials creds;
            if (it->IsScalar())
            {
                creds.url = it->as<std::string>();
            }
            else
            {
                // expecting a map
                auto cred = *it;
                creds.url = URLHandler(cred["url"].as<std::string>());
                if (cred["password"])
                {
                    creds.password = get_env_from_str(cred["password"].as<std::string>(),
                                                      "AWS_SECRET_ACCESS_KEY");
                }
                if (cred["user"])
                {
                    creds.user
                        = get_env_from_str(cred["user"].as<std::string>(), "AWS_ACCESS_KEY_ID");
                }
                if (cred["region"])
                {
                    creds.region
                        = get_env_from_str(cred["region"].as<std::string>(), "AWS_DEFAULT_REGION");
                }
            }
            auto kof = KindOf::kHTTP;
            if (creds.url.scheme() == "s3")
            {
                kof = KindOf::kS3;
            }
            else if (creds.url.scheme() == "oci")
            {
                kof = KindOf::kOCI;
                if (creds.user.empty())
                    creds.user = get_env("GHA_USER", "");
                if (creds.password.empty())
                    creds.password = get_env("GHA_PAT", "");
            }

            if (kof != KindOf::kHTTP)
                creds.url.set_scheme(no_https ? "http" : "https");

            if (kof == KindOf::kS3)
            {
                spdlog::info("Adding S3 mirror: {} -> {}", mirror_name, creds.url.url());
                res[mirror_name].emplace_back(
                    new S3Mirror{ ctx, creds.url.url(), creds.region, creds.user, creds.password });
            }
            else if (kof == KindOf::kOCI)
            {
                spdlog::info("Adding OCI mirror: {} -> {}", mirror_name, creds.url.url());
                if (!creds.password.empty())
                {
                    res[mirror_name].emplace_back(new OCIMirror{ ctx,
                                                                 creds.url.url_without_path(),
                                                                 creds.url.path(),
                                                                 "pull",
                                                                 creds.user,
                                                                 creds.password });
                }
                else
                {
                    res[mirror_name].emplace_back(
                        new OCIMirror{ ctx, creds.url.url_without_path(), creds.url.path() });
                }
                std::dynamic_pointer_cast<OCIMirror>(res[mirror_name].back())
                    ->set_fn_tag_split_function(oci_fn_split_tag);
            }
            else if (kof == KindOf::kHTTP)
            {
                spdlog::info("Adding HTTP mirror: {} -> {}", mirror_name, creds.url.url());
                res[mirror_name].emplace_back(std::make_shared<Mirror>(ctx, creds.url.url()));
            }
        }
    }
    return res;
}


int
main(int argc, char** argv)
{
    CLI::App app;

    bool resume = false;
    std::vector<std::string> du_files;
    std::vector<std::string> mirrors;
    std::string file, outfile, sha_cli, outdir;
    bool verbose = false;
    bool disable_ssl = false;
    bool do_zck_extract = false;
    long int filesize = -1;

    DownloadMetadata dl_meta;

    CLI::App* s_dl = app.add_subcommand("download", "Download a file");
    s_dl->add_option("files", du_files, "Files to download");
    s_dl->add_option("-m", mirrors, "Mirrors from where to download");
    s_dl->add_flag("-r,--resume", resume, "Try to resume");
    s_dl->add_option("-f", file, "File from which to read upload / download files");
    s_dl->add_option("-d", outdir, "Output directory");
    s_dl->add_option("-o", dl_meta.outfile, "Output file");
    s_dl->add_flag("-x", do_zck_extract, "Output file");

    CLI::App* s_ul = app.add_subcommand("upload", "Upload a file");
    s_ul->add_option("files", du_files, "Files to upload");
    s_ul->add_option("-m", mirrors, "Mirror to upload to");
    s_ul->add_option("-f", file, "File from which to read mirrors, upload & download files");

    s_ul->add_flag("-v", verbose, "Enable verbose output");
    s_dl->add_flag("-v", verbose, "Enable verbose output");

    s_dl->add_option("--sha", dl_meta.sha256, "Expected SHA string");
    s_dl->add_option("-i", dl_meta.filesize, "Expected file size");

    s_dl->add_option("--zck-header-sha",
                     dl_meta.zck_header_sha256,
                     "Header SHA256 for zchunk header (find with zck_read_header)");
    s_dl->add_option("--zck-header-size",
                     dl_meta.zck_header_size,
                     "Header size for zchunk header (find with zck_read_header)");
    s_dl->add_flag("-k", disable_ssl, "Disable SSL verification");
    s_ul->add_flag("-k", disable_ssl, "Disable SSL verification");
    s_ul->add_flag("--plain-http", no_https, "Use plain HTTP instead of HTTPS");
    s_dl->add_flag("--plain-http", no_https, "Use plain HTTP instead of HTTPS");

    CLI11_PARSE(app, argc, argv);

    powerloader::Context ctx;

    if (verbose)
    {
        show_progress_bars = false;
        ctx.set_verbosity(1);
    }
    ctx.disable_ssl = disable_ssl;

    std::vector<Mirror> mlist;
    if (!file.empty())
    {
        spdlog::info("Loading file {}", file);
        YAML::Node config = YAML::LoadFile(file);

        du_files = config["targets"].as<std::vector<std::string>>();
        if (config["mirrors"])
        {
            spdlog::info("Loading mirrors", file);
            ctx.mirror_map = parse_mirrors(ctx, config["mirrors"]);
        }
    }
    if (app.got_subcommand("upload"))
    {
        return handle_upload(ctx, du_files, mirrors);
    }
    if (app.got_subcommand("download"))
    {
        return handle_download(ctx, du_files, mirrors, resume, outdir, dl_meta, do_zck_extract);
    }

    return 0;
}
