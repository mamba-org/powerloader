import sys, socket, pytest, py, pathlib
from xprocess import ProcessStarter
from pathlib import Path
import subprocess
import platform
import os
import hashlib
import time
import json
from urllib.request import urlopen
import shutil, yaml, copy
import glob


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
    file_map["sparse_mirrors"] = file_map["test_path"] / Path("sparse_mirrors.yml")

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
    cksums["xtensor-0.24.0-hc021e02_0.tar.bz2"] = "e785d6770ea5e69275c920cb1a6385bf22876e83fe5183a011d53fe705b21980"
    cksums["python-3.9.7-hb7a2778_1_cpython.tar.bz2"] = "6971e6721bbf774a152de720f055d8f9b51439742a09c134698a57a4ed7304ba"
    cksums["xtensor-0.23.10-hd62202e_0.tar.bz2"] = "e47ed847659b646c20d4e3e6162ebc11a53ecfe565928bea4f6c7110333241d5"
    cksums["xtensor-0.23.10-h4bd325d_0.tar.bz2"] = "6440497a44cc09fa43fd6606c2461e52fb3cb3f980e7fe949e332c6a468f024a"
    cksums["xtensor-0.23.10-h2acdbc0_0.tar.bz2"] = "6cfa43e528c21cff3a73b30c48eb04d0332224bd51471a307eea05737c0488d9"
    cksums["xtensor-0.23.10-hc021e02_0.tar.bz2"] = "c21c3cea6517c2f968548b82008a8f418a5d9f47a41ce1cb796574b5f1bdbb67"
    cksums["xtensor-0.23.10-h940c156_0.tar.bz2"] = "cc6a113c98012ee9dbbf695a5ce0d8a4230de8342194766e1259d660d1859f6f"
    cksums["xtensor-0.23.9-h4bd325d_1.tar.bz2"] = "419098106d6c5233f374ec383be6d673d24000c72cf62ca7c56916853b7bdf4f"
    cksums["xtensor-0.23.9-hd62202e_1.tar.bz2"] = "58c515f6be3aa1cef8cd047068751cab92f7cc525e9d62672e009b104d06f9de"
    cksums["xtensor-0.23.9-h2acdbc0_1.tar.bz2"] = "70f65c25f8a8c3879923cd01cffc32c603d7675e7657fa9ca265f5565b9203fd"
    cksums["xtensor-0.23.9-hc021e02_1.tar.bz2"] = "404a2e4664a1cbf94f5f98deaf568b267b7474c4e1267deb367b8c758fe71ed2"
    return cksums


def mock_server(xprocess, name, port):
    curdir = pathlib.Path(__file__).parent
    print("Starting mock_server")

    class Starter(ProcessStarter):
        pattern = "Server started!"
        terminate_on_interrupt = True
        args = [sys.executable, "-u", curdir / 'server.py', '-p', str(port)]

        def startup_check(self):
            s = socket.socket()
            address = 'localhost'
            error = False
            try:
                s.connect((address, port))
            except Exception as e:
                print("something's wrong with %s:%d. Exception is %s" % (address, port, e))
                error = True
            finally:
                s.close()

            return (not error)

    # ensure process is running and return its logfile
    logfile = xprocess.ensure(name, Starter)

    yield f"http://localhost:{port}"  # True

    # clean up whole process tree afterwards
    xprocess.getinfo(name).terminate()

@pytest.fixture
def mock_server_1(xprocess):
    yield from mock_server(xprocess, "m1", 5000)

@pytest.fixture
def mock_server_2(xprocess):
    yield from mock_server(xprocess, "m2", 5001)

@pytest.fixture
def mock_server_3(xprocess):
    yield from mock_server(xprocess, "m3", 5002)

@pytest.fixture
def mock_server_4(xprocess):
    yield from mock_server(xprocess, "m4", 5003)

@pytest.fixture
def yml_content(file):
    with open(file["mirrors"], "r") as stream:
        try:
            return yaml.safe_load(stream)
        except yaml.YAMLError as exc:
            print(exc)

@pytest.fixture
def yml_with_names(yml_content):
    names = []
    for target in yml_content["targets"]:
        names.append(Path(target.split("/")[-1]))
    content = copy.deepcopy(yml_content)
    content["names"] = names
    return content

class TestAll:
    @classmethod
    def setup_class(cls):
        pass

    @classmethod
    def teardown_class(cls):
        pass

    def get_files(self, file):
        return glob.glob(str(file["tmp_path"]) + "/*")

    def remove_all(self, file):
        Path(file["output_path"]).unlink(missing_ok=True)
        Path(file["output_path_pdpart"]).unlink(missing_ok=True)

        for fle in self.get_files(file):
            fle.unlink()

    def calculate_sha256(self, file):
        with open(file, "rb") as f:
            # read entire file as bytes
            b = f.read()
            readable_hash = hashlib.sha256(b).hexdigest();
            return readable_hash

    # Download the expected file
    def test_working_download(self, file, powerloader_binary, mock_server_1, checksums):
        self.remove_all(file)

        # print(mock_server_1 + "/static/packages/" + file['name'])
        out = subprocess.check_output([powerloader_binary, "download",
                                       f"{mock_server_1}/static/packages/{file['name']}",
                                       "-o", file["output_path"]])

        assert self.calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert Path(file["output_path"]).exists()
        assert not Path(file["output_path_pdpart"]).exists()
        assert os.path.getsize(file["output_path"]) == file["size"]

    # Download from a path that works on the third try
    def test_broken_for_three_tries(self, file, powerloader_binary, mock_server_1, checksums):
        self.remove_all(file)
        out = subprocess.check_output([powerloader_binary, "download",
                                       f"{mock_server_1}/broken_counts/static/packages/{file['name']}",
                                       "-o", file["output_path"]])
        assert self.calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert os.path.getsize(file["output_path"]) == file["size"]


    def test_working_download_broken_checksum(self, file, powerloader_binary, mock_server_1):
        self.remove_all(file)
        try:
            out = subprocess.check_output([powerloader_binary, "download",
                                           f"{mock_server_1}/static/packages/{file['name']}",
                                           "--sha", "broken_checksum",
                                           "-o", file["output_path"]])
        except subprocess.CalledProcessError as e: print(e)
        assert not Path(file["output_path_pdpart"]).exists()
        assert not Path(file["output_path"]).exists()

    # Download a broken file
    def test_broken_download_good_checksum(self, file, powerloader_binary, mock_server_1):
        self.remove_all(file)
        try:
            out = subprocess.check_output([powerloader_binary, "download",
                                           f"{mock_server_1}/harm_checksum/static/packages/{file['name']}",
                                           "--sha", "broken_checksum",
                                           "-o", file["output_path"]
                                           ])
        except subprocess.CalledProcessError as e: print(e)

        assert not Path(file["output_path_pdpart"]).exists()
        assert not Path(file["output_path"]).exists()

    def get_prev_headers(self, mock_server_1):
        with urlopen(f"{mock_server_1}/prev_headers") as fi:
            return json.loads(fi.read().decode('utf-8'))

    def test_part_resume(self, file, powerloader_binary, mock_server_1, checksums):
        # Download the expected file
        out = subprocess.check_output([powerloader_binary, "download",
                                       f"{mock_server_1}/static/packages/{file['name']}",
                                       "-o", file["output_path"]])

        with open(file['output_path'], 'rb') as fi:
            data = fi.read()
        with open(file['output_path_pdpart'], 'wb') as fo:
            fo.write(data[0:400])

        # Resume the download
        out = subprocess.check_output([powerloader_binary, "download",
                                    "-r", f"{mock_server_1}/static/packages/{file['name']}",
                                    "-o", file["output_path"]])
        assert self.calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert os.path.getsize(file["output_path"]) == file["size"]

        sent_headers = self.get_prev_headers(mock_server_1)
        assert ('Range' in sent_headers)
        assert (sent_headers['Range'] == 'bytes=400-')


    def test_yml_download_working(self, file, yml_with_names, checksums, powerloader_binary, mock_server_1, mock_server_2):
        self.remove_all(file)

        out = subprocess.check_output([powerloader_binary, "download",
                                       "-f", file["mirrors"],
                                       "-d", file["tmp_path"]])

        for fn in yml_with_names["names"]:
            assert self.calculate_sha256(file["tmp_path"] / fn) == checksums[str(fn)]


    def test_yml_content_based_behavior(self, file, yml_with_names, checksums, powerloader_binary,
                                 mock_server_1, mock_server_2, mock_server_3, mock_server_4):
        self.remove_all(file)

        out = subprocess.check_output([powerloader_binary, "download",
                                       "-f", file["sparse_mirrors"],
                                       "-d", file["tmp_path"]])

        for fn in yml_with_names["names"]:
            assert self.calculate_sha256(file["tmp_path"] / fn) == checksums[str(fn)]
