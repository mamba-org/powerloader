import shutil
import pytest, py
import subprocess
import platform
import datetime
from helpers import *
# import time


@pytest.fixture
def get_proj_root(cwd=os.getcwd()):
    proj_root = cwd
    if not Path(proj_root).exists():
        print("POWERLOADER NOT FOUND!")
    return proj_root


@pytest.fixture
def powerloader_binary(get_proj_root):
    env_var = os.environ.get("POWERLOADER_EXE")
    if env_var:
        return env_var
    else:
        if platform.system() == "Windows":
            return Path(get_proj_root) / "build" / "powerloader.exe"
        else:
            return Path(get_proj_root) / "build" / "powerloader"


@pytest.fixture
def file(get_proj_root, name="xtensor-0.24.0-hc021e02_0.tar.bz2"):
    file_map = {}
    file_map["name"] = name
    file_map["location"] = Path(get_proj_root)
    file_map["server"] = file_map["location"] / "server.py"
    file_map["url"] = "https://beta.mamba.pm/get/conda-forge/osx-arm64/" + file_map["name"]
    file_map["size"] = 185929
    file_map["test_path"] = file_map["location"] / Path("test")
    file_map["tmp_path"] = file_map["test_path"] / Path("tmp")
    file_map["output_path"] = file_map["tmp_path"] / file_map["name"]
    file_map["output_path_pdpart"] = file_map["tmp_path"] / Path(str(file_map["name"]) + ".pdpart")
    file_map["mirrors"] = file_map["test_path"] / Path("mirrors.yml")
    file_map["local_mirrors"] = file_map["test_path"] / Path("local_static_mirrors.yml")
    file_map["authentication"] = file_map["test_path"] / Path("passwd_format_one.yml")
    file_map["s3test"] = file_map["test_path"] / Path("s3test.yml")
    file_map["amazon_server"] = "s3://powerloadertestbucket.s3.eu-central-1.amazonaws.com"
    file_map["s3_yml_template"] = file_map["test_path"] / Path("s3template.yml")

    try:
        os.mkdir(file_map["tmp_path"])
    except OSError:
        print("Creation of the directory %s failed" % file_map["tmp_path"])
    else:
        print("Successfully created the directory %s " % file_map["tmp_path"])

    yield file_map

    shutil.rmtree(file_map["tmp_path"])


@pytest.fixture
def checksums():
    cksums = {}
    cksums["xtensor-0.24.0-hc021e02_0.tar.bz2"] = \
        "e785d6770ea5e69275c920cb1a6385bf22876e83fe5183a011d53fe705b21980"
    cksums["python-3.9.7-hb7a2778_1_cpython.tar.bz2"] = \
        "6971e6721bbf774a152de720f055d8f9b51439742a09c134698a57a4ed7304ba"
    cksums["xtensor-0.23.10-hd62202e_0.tar.bz2"] = \
        "e47ed847659b646c20d4e3e6162ebc11a53ecfe565928bea4f6c7110333241d5"
    cksums["xtensor-0.23.10-h4bd325d_0.tar.bz2"] = \
        "6440497a44cc09fa43fd6606c2461e52fb3cb3f980e7fe949e332c6a468f024a"
    cksums["xtensor-0.23.10-h2acdbc0_0.tar.bz2"] = \
        "6cfa43e528c21cff3a73b30c48eb04d0332224bd51471a307eea05737c0488d9"
    cksums["xtensor-0.23.10-hc021e02_0.tar.bz2"] = \
        "c21c3cea6517c2f968548b82008a8f418a5d9f47a41ce1cb796574b5f1bdbb67"
    cksums["xtensor-0.23.10-h940c156_0.tar.bz2"] = \
        "cc6a113c98012ee9dbbf695a5ce0d8a4230de8342194766e1259d660d1859f6f"
    cksums["xtensor-0.23.9-h4bd325d_1.tar.bz2"] = \
        "419098106d6c5233f374ec383be6d673d24000c72cf62ca7c56916853b7bdf4f"
    cksums["xtensor-0.23.9-hd62202e_1.tar.bz2"] = \
        "58c515f6be3aa1cef8cd047068751cab92f7cc525e9d62672e009b104d06f9de"
    cksums["xtensor-0.23.9-h2acdbc0_1.tar.bz2"] = \
        "70f65c25f8a8c3879923cd01cffc32c603d7675e7657fa9ca265f5565b9203fd"
    cksums["xtensor-0.23.9-hc021e02_1.tar.bz2"] = \
        "404a2e4664a1cbf94f5f98deaf568b267b7474c4e1267deb367b8c758fe71ed2"
    cksums["boa-0.8.1.tar.gz"] = \
        "b824237d80155efd97b79469534d602637b40a2a27c4f71417d5e6977238ff74"
    return cksums


@pytest.fixture
def mock_server_404(xprocess, checksums):
    port = 5001
    pkgs = get_pkgs(port, checksums)
    yield from mock_server(xprocess, "m1", port, pkgs, error_type="404")


@pytest.fixture
def mock_server_lazy(xprocess, checksums):
    port = 5002
    pkgs = get_pkgs(port, checksums)
    yield from mock_server(xprocess, "m2", port, pkgs, error_type="lazy")


@pytest.fixture
def mock_server_broken(xprocess, checksums):
    port = 5003
    pkgs = get_pkgs(port, checksums)
    yield from mock_server(xprocess, "m3", port, pkgs, error_type="broken")


@pytest.fixture
def mock_server_working(xprocess, checksums):
    port = 5004
    pkgs = {}
    yield from mock_server(xprocess, "m4", port, pkgs, error_type=None)


@pytest.fixture
def mock_server_password(xprocess, checksums):
    port = 5005
    pkgs = {}
    yield from mock_server(xprocess, "m5", port, pkgs, error_type=None,
                           uname="user", pwd="secret")


@pytest.fixture
def mirrors_with_names(file):
    return add_names(file, target="mirrors")


@pytest.fixture
def sparse_mirrors_with_names(file):
    return add_names(file, target="local_mirrors")



class TestAll:
    @classmethod
    def setup_class(cls):
        pass

    @classmethod
    def teardown_class(cls):
        pass

    # Download the expected file
    def test_working_download(self, file, powerloader_binary, mock_server_working, checksums):
        remove_all(file)

        # print(mock_server_working + "/static/packages/" + file['name'])
        out = subprocess.check_output([powerloader_binary, "download",
                                       f"{mock_server_working}/static/packages/{file['name']}",
                                       "-o", file["output_path"]])

        assert calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert Path(file["output_path"]).exists()
        assert not Path(file["output_path_pdpart"]).exists()
        assert os.path.getsize(file["output_path"]) == file["size"]

    # Download the expected file
    def test_working_download_pwd(self, file, powerloader_binary, mock_server_password, checksums):
        remove_all(file)

        out = subprocess.check_output([powerloader_binary, "download",
                                       f"{mock_server_password}/static/packages/{file['name']}",
                                       "-o", file["output_path"]])

        assert calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert Path(file["output_path"]).exists()
        assert not Path(file["output_path_pdpart"]).exists()
        assert os.path.getsize(file["output_path"]) == file["size"]

    # Download from a path that works on the third try
    def test_broken_for_three_tries(self, file, powerloader_binary, mock_server_working, checksums):
        remove_all(file)
        out = subprocess.check_output([powerloader_binary, "download",
                                       f"{mock_server_working}/broken_counts/static/packages/{file['name']}",
                                       "-o", file["output_path"]])
        assert calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert os.path.getsize(file["output_path"]) == file["size"]

    def test_working_download_broken_checksum(self, file, powerloader_binary, mock_server_working):
        remove_all(file)
        try:
            out = subprocess.check_output([powerloader_binary, "download",
                                           f"{mock_server_working}/static/packages/{file['name']}",
                                           "--sha", "broken_checksum",
                                           "-o", file["output_path"]])
        except subprocess.CalledProcessError as e:
            print(e)
        assert not Path(file["output_path_pdpart"]).exists()
        assert not Path(file["output_path"]).exists()

    # Download a broken file
    def test_broken_download_good_checksum(self, file, powerloader_binary, mock_server_working):
        remove_all(file)
        try:
            out = subprocess.check_output([powerloader_binary, "download",
                                           f"{mock_server_working}/harm_checksum/static/packages/{file['name']}",
                                           "--sha", "broken_checksum",
                                           "-o", file["output_path"]
                                           ])
        except subprocess.CalledProcessError as e:
            print(e)

        assert not Path(file["output_path_pdpart"]).exists()
        assert not Path(file["output_path"]).exists()

    def test_part_resume(self, file, powerloader_binary, mock_server_working, checksums):
        # Download the expected file
        out = subprocess.check_output([powerloader_binary, "download",
                                       f"{mock_server_working}/static/packages/{file['name']}",
                                       "-o", file["output_path"]])

        with open(file['output_path'], 'rb') as fi:
            data = fi.read()
        with open(file['output_path_pdpart'], 'wb') as fo:
            fo.write(data[0:400])

        # Resume the download
        out = subprocess.check_output([powerloader_binary, "download",
                                       "-r", f"{mock_server_working}/static/packages/{file['name']}",
                                       "-o", file["output_path"]])
        assert calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert os.path.getsize(file["output_path"]) == file["size"]

        sent_headers = get_prev_headers(mock_server_working)
        assert ('Range' in sent_headers)
        assert (sent_headers['Range'] == 'bytes=400-')

    def test_yml_download_working(self, file, mirrors_with_names, checksums, powerloader_binary,
                                  mock_server_working, mock_server_404, mock_server_lazy,
                                  mock_server_broken, mock_server_password):
        remove_all(file)

        out = subprocess.check_output([powerloader_binary, "download",
                                       "-f", file["mirrors"],
                                       "-d", file["tmp_path"]])

        for fn in mirrors_with_names["names"]:
            assert calculate_sha256(file["tmp_path"] / fn) == checksums[str(fn)]

    def test_yml_content_based_behavior(self, file, sparse_mirrors_with_names, checksums, powerloader_binary,
                                        mock_server_working, mock_server_404, mock_server_lazy,
                                        mock_server_broken, mock_server_password):
        remove_all(file)

        out = subprocess.check_output([powerloader_binary, "download",
                                       "-f", file["local_mirrors"],
                                       "-d", file["tmp_path"]])

        for fn in sparse_mirrors_with_names["names"]:
            assert calculate_sha256(file["tmp_path"] / fn) == checksums[str(fn)]

    def test_yml_password_format_one(self, file, sparse_mirrors_with_names, checksums, powerloader_binary,
                                     mock_server_working, mock_server_404, mock_server_lazy,
                                     mock_server_broken, mock_server_password):
        remove_all(file)

        out = subprocess.check_output([powerloader_binary, "download",
                                       "-f", file["authentication"],
                                       "-d", file["tmp_path"]])

        for fn in sparse_mirrors_with_names["names"]:
            assert calculate_sha256(file["tmp_path"] / fn) == checksums[str(fn)]

    @pytest.mark.skipif(os.environ.get("AWS_ACCESS_KEY") is None
                        or os.environ.get("AWS_ACCESS_KEY") == ""
                        or os.environ.get("AWS_SECRET_KEY") is None
                        or os.environ.get("AWS_SECRET_KEY") == ""
                        or os.environ.get("AWS_DEFAULT_REGION") is None
                        or os.environ.get("AWS_DEFAULT_REGION") == "",
                        reason="Environment variable(s) not defined")
    def test_yml_s3_mirror(self, file, checksums, powerloader_binary):
        remove_all(file)
        out = subprocess.check_output([powerloader_binary, "download",
                                       "-f", file["s3test"],
                                       "-d", file["tmp_path"]])

        for fp in get_files(file):
            assert calculate_sha256(fp) == checksums[str(path_to_name(fp))]

    @pytest.mark.skipif(os.environ.get("AWS_ACCESS_KEY") is None
                        or os.environ.get("AWS_ACCESS_KEY") == ""
                        or os.environ.get("AWS_SECRET_KEY") is None
                        or os.environ.get("AWS_SECRET_KEY") == ""
                        or os.environ.get("AWS_DEFAULT_REGION") is None
                        or os.environ.get("AWS_DEFAULT_REGION") == "",
                        reason="Environment variable(s) not defined")
    def test_s3_upload(self, file, powerloader_binary):
        remove_all(file)

        # Generate a unique file
        upload_path = str(file["tmp_path"] / Path(str(platform.system()) + "_test.txt"))
        with open(upload_path, "w+") as f:
            f.write("Content: " + str(datetime.datetime.now()))
        f.close()

        # Store the checksum for later
        hash_before_upload = calculate_sha256(upload_path)

        # Upload the file
        name_on_server = path_to_name(upload_path)
        proc = subprocess.Popen([powerloader_binary, "upload",
                                       upload_path + ":" + name_on_server,
                                       "-m", file["amazon_server"]],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = proc.communicate()
        # assert proc.returncode == 0  # Check that the error code is one

        # Delete the file
        Path(upload_path).unlink()

        # Generate a YML file for the download
        aws_template = yml_content(file["s3_yml_template"])
        aws_template["targets"] = [aws_template["targets"][0].replace("__filename__", name_on_server)]
        print(str(aws_template))

        tmp_yaml = file["tmp_path"] / Path("tmp.yml")
        with open(str(tmp_yaml), 'w') as outfile:
            yaml.dump(aws_template, outfile, default_flow_style=False)

        # Download using this YML file
        proc = subprocess.Popen([powerloader_binary, "download",
                                       "-f", str(tmp_yaml),
                                       "-d", str(file["tmp_path"])],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = proc.communicate()
        assert proc.returncode == 0

        # Check that the downloaded file is the same as the uploaded file
        hash_after_upload = calculate_sha256(upload_path)
        assert hash_before_upload == hash_after_upload

    # TODO: Parse outputs?, Randomized tests?
    def test_yml_with_interruptions(self, file, sparse_mirrors_with_names, checksums, powerloader_binary,
                                    mock_server_working, mock_server_404, mock_server_lazy,
                                    mock_server_broken, mock_server_password):
        remove_all(file)
        out = subprocess.check_output([powerloader_binary, "download",
                                       "-f", file["local_mirrors"],
                                       "-d", file["tmp_path"], "-v"])

        pdp = ".pdpart"
        for fn in sparse_mirrors_with_names["names"]:
            fp = file["tmp_path"] / fn
            with open(fp, 'rb') as fi:
                data = fi.read()

            with open(str(fp) + pdp, 'wb') as fo:
                fo.write(data[0:400])
            fp.unlink()

        # The servers is reliable now
        for broken_file in filter_broken(get_files(file), pdp):
            fn = path_to_name(broken_file.replace(pdp, ""))
            fp = Path(file["tmp_path"]) / Path(fn)
            out = subprocess.check_output([powerloader_binary, "download",
                                           "-r", f"{mock_server_working}/static/packages/{fn}",
                                           "-o", fp])

        for fn in sparse_mirrors_with_names["names"]:
            assert calculate_sha256(file["tmp_path"] / fn) == checksums[str(fn)]

        print(file["test_path"])

    def test_zchunk_basic(file, powerloader_binary, mock_server_working):
        # Download the expected file
        assert (not Path('lorem.txt.zck').exists())

        out = subprocess.check_output([powerloader_binary,
                                       "download",
                                       f"{mock_server_working}/static/zchunk/lorem.txt.zck",
                                       "--zck-header-size", "257",
                                       "--zck-header-sha",
                                       "57937bf55851d111a497c1fe2ad706a4df70e02c9b8ba3698b9ab5f8887d8a8b"])

        assert (Path('lorem.txt.zck').exists())
        Path('lorem.txt.zck').unlink()

    def test_zchunk_basic_extract(file, powerloader_binary, mock_server_working):
        # Download the expected file
        assert (not Path('lorem.txt.zck').exists())
        assert (not Path('lorem.txt').exists())

        out = subprocess.check_output([powerloader_binary,
                                       "download",
                                       f"{mock_server_working}/static/zchunk/lorem.txt.zck",
                                       "-x", "--zck-header-size", "257",
                                       "--zck-header-sha",
                                       "57937bf55851d111a497c1fe2ad706a4df70e02c9b8ba3698b9ab5f8887d8a8b"])

        assert (Path('lorem.txt.zck').exists())
        assert (Path('lorem.txt').exists())
        Path('lorem.txt.zck').unlink()
        Path('lorem.txt').unlink()

    def test_zchunk_aws(file, powerloader_binary, mock_server_working):
        # Download the expected file
        assert (not Path('lorem.txt.zck').exists())
        assert (not Path('lorem.txt').exists())

        out = subprocess.check_output([powerloader_binary,
                                       "download",
                                       f"{mock_server_working}/static/zchunk/lorem.txt.zck",
                                       "-x", "--zck-header-size", "257",
                                       "--zck-header-sha",
                                       "57937bf55851d111a497c1fe2ad706a4df70e02c9b8ba3698b9ab5f8887d8a8b"])

        assert (Path('lorem.txt.zck').exists())
        assert (Path('lorem.txt').exists())
        Path('lorem.txt.zck').unlink()
        Path('lorem.txt').unlink()

    """
    def test_zchunk_aws(file, powerloader_binary, mock_server_working):
        raise Exception("Stop here")

    def test_zchunk_oci(file, powerloader_binary, mock_server_working):
        raise Exception("Stop here")
    """

    def test_zchunk_random_file(self, file):
        remove_all(file)
        name = "_random_file"

        path1 = str(Path(file["tmp_path"]) / Path(str(platform.system()) + name + "1.txt"))
        path2 = str(Path(file["tmp_path"]) / Path(str(platform.system()) + name + "2.txt"))
        path3 = str(Path(file["tmp_path"]) / Path(str(platform.system()) + name + "3.txt"))
        exponent = 20
        generate_random_file(path1, size=2**exponent)
        generate_random_file(path2, size=2**exponent)

        # opening first file in append mode and second file in read mode
        f1 = open(path1, 'rb')
        f2 = open(path2, 'rb')
        f3 = open(path3, 'a+b')

        # appending the contents of the second file to the first file
        f3.write(f1.read())
        f3.write(f2.read())

        # Comput zchunks
        out1 = subprocess.check_output(["zck", path1, "-o", path1 + ".zck"])
        out2 = subprocess.check_output(["zck", path2, "-o", path2 + ".zck"])
        out3 = subprocess.check_output(["zck", path3, "-o", path3 + ".zck"])

        # Check delta size
        dsize1 = subprocess.check_output(["zck_delta_size", path1 + ".zck", path3 + ".zck"])
        dsize2 = subprocess.check_output(["zck_delta_size", path2 + ".zck", path3 + ".zck"])

        print(dsize1)

        pf_1, pch_1, num_chunks_1 = get_percentage(dsize1)
        pf_2, pch_2, num_chunks_2 = get_percentage(dsize2)

        print("Will download " + str(round(pf_1)) + "% of file1, that's " + str(round(pch_1)) + "% of chunks. Total: " + str(num_chunks_1) + " chunks.")
        print("Will download " + str(round(pf_2)) + "% of file2, that's " + str(round(pch_2)) + "% of chunks. Total: " + str(num_chunks_2) + " chunks.")

        # Compute zchunk of path1
        # path3 = Concatunate path1 & path2
        # compute zchunk of path3
        # zck_delta_size: confirm that the difference is 2**16 bytes in size
