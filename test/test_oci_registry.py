from pydoc import plain
from fixtures import *
from xprocess import ProcessStarter


def proj_root(cwd=os.getcwd()):
    proj_root = cwd
    if not Path(proj_root).exists():
        print("POWERLOADER NOT FOUND!")
    return proj_root


def get_powerloader_binary():
    env_var = os.environ.get("POWERLOADER_EXE")
    if env_var:
        return env_var
    else:
        if platform.system() == "Windows":
            return Path(proj_root()) / "build" / "powerloader.exe"
        else:
            return Path(proj_root()) / "build" / "powerloader"


def get_oci_path(name_on_server, tag):
    newname = name_on_server + "-" + tag
    newpath = file["tmp_path"] / Path(newname)
    return newname, newpath


def generate_oci_download_yml(name_on_server, tag, server, tmp_folder):
    download_name = f"{name_on_server}-{tag}"
    oci = {
        "mirrors": {"ocitest": [f"oci://{server.split('://')[1]}"]},
        "targets": [f"ocitest:{download_name}"],
    }

    tmp_yaml = tmp_folder / Path("tmp.yml")
    with open(str(tmp_yaml), "w") as outfile:
        yaml.dump(oci, outfile, default_flow_style=False)
    return tmp_yaml, download_name


def download_oci_file(tmp_yaml, tmp_folder, server, plain_http=False):
    server = f"oci://{server.split('://')[1]}"
    plb = get_powerloader_binary()
    command = [
        plb,
        "download",
        "-f",
        str(tmp_yaml),
        "-d",
        str(tmp_folder),
    ]

    if plain_http != False:
        command.extend(["-k", "-v", "--plain-http"])

    run_command(command, ok_in=server.split("://")[1])


def oci_check_present(uploc, srvname, tag, expect=True):
    if gha_credentials_exist():
        # TODO: Github doesn't support `/v2/_catalog` yet
        # https://github.community/t/ghcr-io-docker-http-api/130121/3
        pass
    else:
        oci_file_presence(uploc, srvname, tag, expect)
        if expect == True:
            path = uploc.replace("oci://", "http://")
            path += "/v2/" + srvname + "/tags/list"
            tags = requests.get(path).json()
            assert tag in set(tags["tags"])


def oci_file_presence(uploc, srvname, tag, expect):
    path = uploc.replace("oci://", "http://") + "/v2/_catalog"
    repos = requests.get(path).json()
    assert (srvname in set(repos["repositories"])) == expect


def upload_oci(upload_path, tag, server, plain_http=False):
    server = f"oci://{server.split('://')[1]}"

    # TODO: use powerloader_binary from fixtures.py?
    plb = get_powerloader_binary()
    srv_name = path_to_name(upload_path)
    command = [
        plb,
        "upload",
        f"{upload_path}:{srv_name}:{tag}",
        "-m",
        server,
    ]
    if plain_http != False:
        command.extend(["-k", "-v", "--plain-http"])

    out = run_command(command, ok_in=server.split("://")[1])

    oci_check_present(server, srv_name, tag, expect=True)
    return srv_name


def mock_oci_registry_starter(xprocess, name, port):
    curdir = pathlib.Path(__file__).parent
    print("Starting mock_server")

    class Starter(ProcessStarter):

        pattern = "listening on"
        terminate_on_interrupt = True

        args = [
            "docker",
            "run",
            "-p",
            f"{port}:5000",
            "--rm",
            f"--name={name}",
            "registry:2",
        ]

        def startup_check(self):
            s = socket.socket()
            error = False
            try:
                s.connect(("localhost", port))
            except Exception as e:
                print(
                    "something's wrong with localhost:%d. Exception is %s" % (port, e)
                )
                error = True
            finally:
                s.close()
            return not error

    logfile = xprocess.ensure(name, Starter)

    yield f"http://localhost:{port}"

    xprocess.getinfo(name).terminate()


@pytest.fixture
def mock_oci_registry(xprocess, fs_ready):
    yield from mock_oci_registry_starter(xprocess, "mock_oci_registry", 5123)


@pytest.fixture
def zck_file_upload(file, mock_oci_registry):
    tag = "1.0"
    srv_name = upload_oci(
        upload_path=file["lorem_zck_file"],
        tag=tag,
        server=mock_oci_registry,
        plain_http=True,
    )

    srv_name_x3 = upload_oci(
        upload_path=file["lorem_zck_file_x3"],
        tag=tag,
        server=mock_oci_registry,
        plain_http=True,
    )
    yield srv_name, srv_name_x3, tag
    # TODO: Remove files from remote again


@pytest.fixture
def zck_file_upload_ghcr(xprocess, fs_ready, file):
    tag = "1.0"
    srv_name = upload_oci(
        upload_path=file["lorem_zck_file"], tag=tag, server="https://ghcr.io"
    )

    srv_name_x3 = upload_oci(
        upload_path=file["lorem_zck_file_x3"], tag=tag, server="https://ghcr.io"
    )
    yield srv_name, srv_name_x3, tag
    # TODO: Remove files from remote again


@pytest.fixture
def temp_txt_file(tmp_path):
    p = tmp_path / "testfile"
    p.write_text("Content: " + str(datetime.datetime.now()))
    return (p, calculate_sha256(p))


@pytest.fixture
def clean_env():
    pat = os.environ.get("GHA_PAT")
    user = os.environ.get("GHA_USER")

    if pat:
        del os.environ["GHA_PAT"]
    if user:
        del os.environ["GHA_USER"]

    yield

    if pat:
        os.environ["GHA_PAT"] = pat
    if user:
        os.environ["GHA_USER"] = user


def no_docker():
    return shutil.which("docker") is None


skip_no_docker = pytest.mark.skipif(no_docker(), reason="No docker installed")
skip_no_gha_credentials = pytest.mark.skipif(
    not gha_credentials_exist(), reason="GHA credentials are not set"
)
skip_gha_credentials = pytest.mark.skipif(
    not gha_credentials_dont_exist(), reason="GHA credentials are set"
)

skip_no_pat = pytest.mark.skipif(not os.environ.get("GHA_PAT"), reason="No GHA_PAT set")


class TestOCImock:

    # Upload a file
    @skip_no_docker
    def test_upload(self, mock_oci_registry, temp_txt_file, clean_env):
        tag = "1.0"
        name_on_server = upload_oci(
            temp_txt_file[0], tag, mock_oci_registry, plain_http=True
        )

    # Download a file that's always there...
    # def test_download_permanent(
    #     self, file, powerloader_binary, checksums, mock_oci_registry
    # ):
    #     tag, name_on_server, username = oci_path_resolver(
    #         file, username="", name_on_server=file["name_on_mock_server"]
    #     )
    #     Path(get_oci_path(file, name_on_server, tag)[1]).unlink(missing_ok=True)
    #     newpath, tmp_yaml = generate_oci_download_yml(
    #         file, tag, name_on_server, username, local=True
    #     )
    #     download_oci_file(powerloader_binary, tmp_yaml, file, plain_http=True)
    #     assert checksums[file["name_on_mock_server"]] == calculate_sha256(newpath)

    # Upload a file and download it again
    @skip_no_docker
    @skip_gha_credentials
    def test_upload_and_download(
        self, temp_txt_file, powerloader_binary, mock_oci_registry, clean_env
    ):
        # Upload
        temp_folder = temp_txt_file[0].parent
        tag = "1.123"
        name_on_server = upload_oci(
            temp_txt_file[0], tag, mock_oci_registry, plain_http=True
        )
        temp_txt_file[0].unlink()

        # Download
        tmp_yaml, download_name = generate_oci_download_yml(
            name_on_server, tag, mock_oci_registry, temp_folder
        )
        dl_folder = temp_folder / "dl"
        dl_folder.mkdir()

        download_oci_file(tmp_yaml, dl_folder, mock_oci_registry, plain_http=True)
        assert temp_txt_file[1] == calculate_sha256(
            dl_folder / f"{temp_txt_file[0].name}-{tag}"
        )

    @skip_no_pat
    def test_upload_ghcr(self, file, unique_filename):
        upload_path = generate_unique_file(file, unique_filename)
        hash_before_upload = calculate_sha256(upload_path)
        tag = "12.24"
        name_on_server = upload_oci(upload_path, tag, "https://ghcr.io")

    # def test_download_permanent(self, file, checksums):
    #     tag, name_on_server, username = oci_path_resolver(file)
    #     Path(get_oci_path(file, name_on_server, tag)[1]).unlink(missing_ok=True)
    #     newpath, tmp_yaml = generate_oci_download_yml(
    #         file, tag, name_on_server, username
    #     )
    #     download_oci_file(powerloader_binary, tmp_yaml, file)
    #     assert checksums[file["name_on_server"]] == calculate_sha256(newpath)

    def set_username(self):
        username = ""
        if gha_credentials_exist():
            username = os.environ.get("GHA_USER")
        else:
            username = "mamba-org"  # GHA_PAT is only available on the main branch
            os.environ["GHA_USER"] = username  # GHA_USER must also be set
        return username

    # Download a file that's always there...
    @skip_no_pat
    def test_upload_and_download_ghcr(self, temp_txt_file):
        username = self.set_username()
        temp_folder = temp_txt_file[0].parent
        tag = "24.21"
        name_on_server = upload_oci(temp_txt_file[0], tag, "https://ghcr.io")
        temp_txt_file[0].unlink()

        tmp_yaml, download_name = generate_oci_download_yml(
            name_on_server, tag, f"https://ghcr.io/{username}", temp_folder,
        )
        dl_folder = temp_folder / "dl"
        dl_folder.mkdir()

        download_oci_file(tmp_yaml, dl_folder, f"https://ghcr.io/{username}")

        assert temp_txt_file[1] == calculate_sha256(
            dl_folder / f"{temp_txt_file[0].name}-{tag}"
        )

        # TODO: Delete OCI from server
        # Need to figure out what the package id is
        # Delete: https://stackoverflow.com/questions/59103177/how-to-delete-remove-unlink-unversion-a-package-from-the-github-package-registry
        # https://github.com/actions/delete-package-versions

    @skip_no_docker
    @skip_gha_credentials
    def test_zchunk_basic_registry(self, file, zck_file_upload, mock_oci_registry):
        # Download the expected file
        name = file["lorem_zck_file"].name
        dl_folder = file["tmp_path"] / "dl"
        dl_folder.mkdir()
        localpath = file["tmp_path"] / name
        assert not localpath.exists()

        srv_name, srv_name_x3, tag = zck_file_upload
        tmp_yaml, download_name = generate_oci_download_yml(
            srv_name, tag, mock_oci_registry, file["tmp_path"],
        )

        localfile = dl_folder / download_name
        assert not localfile.exists()

        download_oci_file(tmp_yaml, dl_folder, mock_oci_registry, plain_http=True)

        """
        headers = get_prev_headers(mock_server_working, 2)
        assert headers[0]["Range"] == "bytes=0-256"
        assert headers[1]["Range"] == "bytes=257-4822"
        """
        assert not localpath.exists()
        assert localfile.exists()
        assert localfile.stat().st_size == 4823
        assert not Path(str(localfile) + ".pdpart").exists()

        """
        clear_prev_headers(mock_server_working)
        """

        download_oci_file(tmp_yaml, dl_folder, mock_oci_registry, plain_http=True)

        """
        headers = get_prev_headers(mock_server_working, 100)
        assert headers is None
        """

        assert not localpath.exists()
        assert localfile.exists()
        assert localfile.stat().st_size == 4823
        assert not Path(str(localfile) + ".pdpart").exists()

        """
        clear_prev_headers(mock_server_working)
        """

        tmp_yaml, download_name = generate_oci_download_yml(
            srv_name_x3, tag, mock_oci_registry, file["tmp_path"],
        )
        download_oci_file(tmp_yaml, dl_folder, mock_oci_registry, plain_http=True)

        """
        headers = get_prev_headers(mock_server_working, 100)
        assert len(headers) == 2

        # lead
        assert headers[0]["Range"] == "bytes=0-257"
        # header
        range_start = int(headers[1]["Range"][len("bytes=") :].split("-")[0])
        assert range_start > 4000
        """

        # TODO: Specify output path, so that test/tmp/lorem.txt.zck is modified
        # TODO: Download with CLI params rather than YML file, so that the headers can be checked easily
        if False:
            assert not Path(str(localfile) + ".pdpart").exists()
            assert not localpath.exists()
            assert localfile.stat().st_size == file["lorem_zck_file_x3"].stat().st_size
            assert calculate_sha256(file["lorem_zck_file_x3"]) == calculate_sha256(
                str(localfile)
            )
            assert localfile.exists()
            localfile.unlink()
        else:
            new_localpath = Path(str(dl_folder / srv_name_x3) + "-1.0")
            assert new_localpath.exists()
            assert not localpath.exists()
            assert not Path(str(new_localpath) + ".pdpart").exists()
            assert (
                new_localpath.stat().st_size == file["lorem_zck_file_x3"].stat().st_size
            )
            assert calculate_sha256(file["lorem_zck_file_x3"]) == calculate_sha256(
                str(new_localpath)
            )
            new_localpath.unlink()

    @skip_no_pat
    def test_zchunk_basic_ghcr(self, file, zck_file_upload_ghcr):
        # Download the expected file
        name = file["lorem_zck_file"].name
        dl_folder = file["tmp_path"] / "dl"
        dl_folder.mkdir()
        localpath = file["tmp_path"] / name
        assert not localpath.exists()

        username = self.set_username()
        srv_name, srv_name_x3, tag = zck_file_upload_ghcr
        tmp_yaml, download_name = generate_oci_download_yml(
            srv_name, tag, f"https://ghcr.io/{username}", file["tmp_path"],
        )

        localfile = dl_folder / download_name
        assert not localfile.exists()

        download_oci_file(tmp_yaml, dl_folder, f"https://ghcr.io/{username}")

        """
        headers = get_prev_headers(mock_server_working, 2)
        assert headers[0]["Range"] == "bytes=0-256"
        assert headers[1]["Range"] == "bytes=257-4822"
        """
        assert not localpath.exists()
        assert localfile.exists()
        assert localfile.stat().st_size == 4823
        assert not Path(str(localfile) + ".pdpart").exists()

        """
        clear_prev_headers(mock_server_working)
        """

        download_oci_file(tmp_yaml, dl_folder, f"https://ghcr.io/{username}")

        """
        headers = get_prev_headers(mock_server_working, 100)
        assert headers is None
        """

        assert not localpath.exists()
        assert localfile.exists()
        assert localfile.stat().st_size == 4823
        assert not Path(str(localfile) + ".pdpart").exists()

        """
        clear_prev_headers(mock_server_working)
        """

        tmp_yaml, download_name = generate_oci_download_yml(
            srv_name_x3, tag, f"https://ghcr.io/{username}", file["tmp_path"],
        )
        download_oci_file(tmp_yaml, dl_folder, f"https://ghcr.io/{username}")

        """
        headers = get_prev_headers(mock_server_working, 100)
        assert len(headers) == 2

        # lead
        assert headers[0]["Range"] == "bytes=0-257"
        # header
        range_start = int(headers[1]["Range"][len("bytes=") :].split("-")[0])
        assert range_start > 4000
        """

        # TODO: Specify output path, so that test/tmp/lorem.txt.zck is modified
        # TODO: Download with CLI params rather than YML file, so that the headers can be checked easily
        if False:
            assert not Path(str(localfile) + ".pdpart").exists()
            assert not localpath.exists()
            assert localfile.stat().st_size == file["lorem_zck_file_x3"].stat().st_size
            assert calculate_sha256(file["lorem_zck_file_x3"]) == calculate_sha256(
                str(localfile)
            )
            assert localfile.exists()
            localfile.unlink()
        else:
            new_localpath = Path(str(dl_folder / srv_name_x3) + "-1.0")
            assert new_localpath.exists()
            assert not localpath.exists()
            assert not Path(str(new_localpath) + ".pdpart").exists()
            assert (
                new_localpath.stat().st_size == file["lorem_zck_file_x3"].stat().st_size
            )
            assert calculate_sha256(file["lorem_zck_file_x3"]) == calculate_sha256(
                str(new_localpath)
            )
            new_localpath.unlink()

    @skip_no_pat
    def test_growing_file_ghcr(
        self, file, checksums, unique_filename, zchunk_expectations
    ):
        tag = "1.0"
        name = Path("static/zchunk/growing_file/gf" + str(unique_filename) + ".zck")
        remote_zck_path = file["test_path"] / Path("conda_mock") / name
        remote_plain_path = Path(str(remote_zck_path).replace(".zck", ""))
        local_zck_path = file["tmp_path"] / Path(str(name.name) + "-" + tag)
        local_plain_path = Path(
            str(local_zck_path).replace(".zck", "").replace("-" + tag, "")
        )
        remove_all(file)

        content_present, gf = setup_file(
            generate_content(file, checksums), unique_filename
        )
        assert content_present == True

        srv_name = upload_oci(
            upload_path=remote_zck_path, tag=tag, server="https://ghcr.io"
        )
        username = self.set_username()

        tmp_yaml, download_name = generate_oci_download_yml(
            srv_name, tag, f"https://ghcr.io/{username}", file["tmp_path"],
        )

        for i in range(5):
            download_oci_file(tmp_yaml, file["tmp_path"], f"https://ghcr.io/{username}")
            unzck(file["tmp_path"] / local_zck_path.name, ghcr_tag=tag)
            assert calculate_sha256(remote_zck_path) == calculate_sha256(local_zck_path)
            assert calculate_sha256(remote_plain_path) == calculate_sha256(
                local_plain_path
            )
            percentage_map = get_zck_percent_delta(local_zck_path, first_time=(i == 0))
            if percentage_map != False:
                assert zchunk_expectations[i] == percentage_map
                if False:
                    print(
                        "\n\ni: "
                        + str(i)
                        + ", percentage_map: "
                        + str(percentage_map)
                        + "\n expectations[i]: "
                        + str(zchunk_expectations[i])
                    )

                if False:
                    print("i: " + str(i) + ", percentage_map: " + str(percentage_map))
            gf.add_content()
            srv_name = upload_oci(
                upload_path=remote_zck_path, tag=tag, server="https://ghcr.io"
            )
        # TODO: Check headers!

    @skip_no_docker
    @skip_gha_credentials
    def test_growing_file_registry(
        self, file, checksums, unique_filename, zchunk_expectations, mock_oci_registry
    ):
        tag = "1.0"
        name = Path("static/zchunk/growing_file/gf" + str(unique_filename) + ".zck")
        remote_zck_path = file["test_path"] / Path("conda_mock") / name
        remote_plain_path = Path(str(remote_zck_path).replace(".zck", ""))
        local_zck_path = file["tmp_path"] / Path(str(name.name) + "-" + tag)
        local_plain_path = Path(
            str(local_zck_path).replace(".zck", "").replace("-" + tag, "")
        )
        remove_all(file)

        content_present, gf = setup_file(
            generate_content(file, checksums), unique_filename
        )
        assert content_present == True

        name_on_server = upload_oci(
            upload_path=remote_zck_path,
            tag=tag,
            server=mock_oci_registry,
            plain_http=True,
        )

        tmp_yaml, download_name = generate_oci_download_yml(
            name_on_server, tag, mock_oci_registry, file["tmp_path"],
        )

        for i in range(16):
            download_oci_file(
                tmp_yaml, file["tmp_path"], mock_oci_registry, plain_http=True
            )
            unzck(file["tmp_path"] / local_zck_path.name, ghcr_tag=tag)
            assert calculate_sha256(remote_zck_path) == calculate_sha256(local_zck_path)
            assert calculate_sha256(remote_plain_path) == calculate_sha256(
                local_plain_path
            )
            percentage_map = get_zck_percent_delta(local_zck_path, first_time=(i == 0))
            if percentage_map != False:
                assert zchunk_expectations[i] == percentage_map
                if False:
                    print(
                        "\n\ni: "
                        + str(i)
                        + ", percentage_map: "
                        + str(percentage_map)
                        + "\n expectations[i]: "
                        + str(zchunk_expectations[i])
                    )

                if False:
                    print("i: " + str(i) + ", percentage_map: " + str(percentage_map))
            gf.add_content()
            srv_name = upload_oci(
                upload_path=remote_zck_path,
                tag=tag,
                server=mock_oci_registry,
                plain_http=True,
            )
        # TODO: Check headers!
