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
import shutil


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
    file_map["checksum"] = "e785d6770ea5e69275c920cb1a6385bf22876e83fe5183a011d53fe705b21980"
    file_map["size"] = 185929
    file_map["test_path"] = "test/tmp/"
    file_map["output_path"] = file_map["location"] / file_map["test_path"] / file_map["name"]
    file_map["output_path_pdpart"] = file_map["location"] / file_map["test_path"] / Path(str(file_map["name"]) + ".pdpart")

    try:
        os.mkdir(file_map["test_path"])
    except OSError:
        print ("Creation of the directory %s failed" % file_map["test_path"])
    else:
        print ("Successfully created the directory %s " % file_map["test_path"])

    yield file_map

    shutil.rmtree(file_map["test_path"])

@pytest.fixture
def mock_server(xprocess):
    port = 4444
    curdir = pathlib.Path(__file__).parent

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
    logfile = xprocess.ensure("mock_server", Starter)

    yield f"http://localhost:{port}"  # True

    # clean up whole process tree afterwards
    xprocess.getinfo("mock_server").terminate()



class TestAll:
    @classmethod
    def setup_class(cls):
        pass

    @classmethod
    def teardown_class(cls):
        pass

    def calculate_sha256(self, file):
        with open(file, "rb") as f:
            # read entire file as bytes
            b = f.read()
            readable_hash = hashlib.sha256(b).hexdigest();
            return readable_hash

    def remove_all(self, file):
        file["output_path"].unlink(missing_ok=True)
        file["output_path_pdpart"].unlink(missing_ok=True)

    # Download the expected file
    def test_working_download(self, file, powerloader_binary, mock_server):
        self.remove_all(file)
        out = subprocess.check_output([powerloader_binary,
                                       "download",
                                       f"{mock_server}/static/packages/{file['name']}",
                                       "-o",
                                       file["output_path"]])

        assert self.calculate_sha256(file["output_path"]) == file["checksum"]
        assert Path(file["output_path"]).exists()
        assert not Path(file["output_path_pdpart"]).exists()
        assert os.path.getsize(file["output_path"]) == file["size"]


    # Download from a path that works on the third try
    def test_broken_for_three_tries(self, file, powerloader_binary, mock_server):
        self.remove_all(file)
        out = subprocess.check_output([powerloader_binary,
                                       "download",
                                       f"{mock_server}/broken_counts/static/packages/{file['name']}",
                                       "-o",
                                       file["output_path"]])
        assert self.calculate_sha256(file["output_path"]) == file["checksum"]
        assert os.path.getsize(file["output_path"]) == file["size"]


    def test_working_download_broken_checksum(self, file, powerloader_binary, mock_server):
        self.remove_all(file)
        try:
            out = subprocess.check_output([powerloader_binary,
                                           "download",
                                           f"{mock_server}/static/packages/{file['name']}",
                                           "--sha",
                                           "broken_checksum",
                                           "-o",
                                           file["output_path"]])
        except subprocess.CalledProcessError as e: print(e)
        assert not Path(file["output_path_pdpart"]).exists()
        assert not Path(file["output_path"]).exists()

    # Download a broken file
    def test_broken_download_good_checksum(self, file, powerloader_binary, mock_server):
        self.remove_all(file)
        try:
            out = subprocess.check_output([powerloader_binary,
                                           "download",
                                           f"{mock_server}/harm_checksum/static/packages/{file['name']}",
                                           "--sha",
                                           "broken_checksum",
                                           "-o",
                                           file["output_path"]
                                           ])
        except subprocess.CalledProcessError as e: print(e)

        assert not Path(file["output_path_pdpart"]).exists()
        assert not Path(file["output_path"]).exists()

    def get_prev_headers(self, mock_server):
        with urlopen(f"{mock_server}/prev_headers") as fi:
            return json.loads(fi.read().decode('utf-8'))

    def test_part_resume(self, file, powerloader_binary, mock_server):
        # Download the expected file
        out = subprocess.check_output([powerloader_binary,
                                       "download",
                                       f"{mock_server}/static/packages/{file['name']}",
                                       "-o",
                                       file["output_path"]])

        with open(file['output_path'], 'rb') as fi:
            data = fi.read()
        with open(file['output_path_pdpart'], 'wb') as fo:
            fo.write(data[0:400])

        # Resume the download
        out = subprocess.check_output([powerloader_binary,
                                   "download", "-r",
                                   f"{mock_server}/static/packages/{file['name']}",
                                   "-o",
                                   file["output_path"]])
        assert self.calculate_sha256(file["output_path"]) == file["checksum"]
        assert os.path.getsize(file["output_path"]) == file["size"]

        sent_headers = self.get_prev_headers(mock_server)
        assert ('Range' in sent_headers)
        assert (sent_headers['Range'] == 'bytes=400-')
