from fixtures import *
from xprocess import ProcessStarter


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
    return tmp_yaml


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

    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate()

    print(out.decode("utf-8"), err.decode("utf-8"))
    err_ok = server.split("://")[1] in str(out.decode("utf-8"))
    assert (err == "".encode("utf-8")) or err_ok
    assert proc.returncode == 0


def oci_check_present(uploc, srvname, tag, expect=True):
    if gha_credentials_exist():
        # Github doesn't support `/v2/_catalog` yet
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
    print(repos)
    assert (srvname in set(repos["repositories"])) == expect


def upload_oci(upload_path, tag, server, plain_http=False):
    server = f"oci://{server.split('://')[1]}"

    plb = get_powerloader_binary()
    srv_name = path_to_name(upload_path)
    command = [
        plb,
        "upload",
        f"{upload_path}:{srv_name}:{tag}",
        "-m",
        server,
    ]
    print("Uploading ", upload_path, srv_name, tag)
    if plain_http != False:
        command.extend(["-k", "-v", "--plain-http"])
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate()

    oci_check_present(server, srv_name, tag, expect=True)

    assert "error" not in str(out.decode("utf-8"))
    err_ok = server.split("://")[1] in str(out.decode("utf-8"))
    assert (err == "".encode("utf-8")) or err_ok
    assert proc.returncode == 0
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
                    "something's wrong with %s:%d. Exception is %s" % (address, port, e)
                )
                error = True
            finally:
                s.close()
            return not error

    logfile = xprocess.ensure(name, Starter)

    yield f"http://localhost:{port}"

    xprocess.getinfo(name).terminate()


@pytest.fixture
def mock_oci_registry(xprocess):
    yield from mock_oci_registry_starter(xprocess, "mock_oci_registry", 5123)


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


class TestOCImock:

    # Upload a file
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
        tmp_yaml = generate_oci_download_yml(
            name_on_server, tag, mock_oci_registry, temp_folder
        )
        dl_folder = temp_folder / "dl"
        dl_folder.mkdir()

        print(dl_folder)

        download_oci_file(tmp_yaml, dl_folder, mock_oci_registry, plain_http=True)
        assert temp_txt_file[1] == calculate_sha256(
            dl_folder / f"{temp_txt_file[0].name}-{tag}"
        )

    @pytest.mark.skipif(not os.environ.get("GHA_PAT"), reason="No GHA_PAT set")
    def test_upload_ghcr(self, file):
        upload_path = generate_unique_file(file)
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
    @pytest.mark.skipif(not os.environ.get("GHA_PAT"), reason="No GHA_PAT set")
    def test_upload_and_download_ghcr(self, temp_txt_file):
        username = self.set_username()
        temp_folder = temp_txt_file[0].parent
        tag = "24.21"
        name_on_server = upload_oci(temp_txt_file[0], tag, "https://ghcr.io")
        temp_txt_file[0].unlink()

        tmp_yaml = generate_oci_download_yml(
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
