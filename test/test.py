import sys, socket, pytest, py, pathlib
from xprocess import ProcessStarter
from pathlib import Path
import subprocess
import platform
import os
import hashlib
import time

perform_slow_tests = False

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
    file_map["location"] = get_proj_root + "/"
    file_map["path"] = file_map["location"] + file_map["name"]
    file_map["pdpart_path"] = file_map["path"] + ".pdpart"
    file_map["server"] = file_map["location"] + "server.py"
    file_map["url"] = "https://beta.mamba.pm/get/conda-forge/osx-arm64/" + file_map["name"]
    file_map["checksum"] = "e785d6770ea5e69275c920cb1a6385bf22876e83fe5183a011d53fe705b21980"
    file_map["size"] = 185929
    return file_map


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

    yield f"http://localhost:{port}" # True

    # clean up whole process tree afterwards
    xprocess.getinfo("mock_server").terminate()


def remove_file(file_path):
    args = ("rm", "-rf", file_path)
    subprocess.call('%s %s %s' % args, shell=True)
    assert not Path(file_path).exists()


def calculate_sha256(file):
    with open(file, "rb") as f:
        # read entire file as bytes
        b = f.read()
        readable_hash = hashlib.sha256(b).hexdigest();
        return readable_hash


def remove_all(file):
    remove_file(file["path"])
    remove_file(file["pdpart_path"])


# Download the expected file
def test_working_download(file, powerloader_binary, mock_server):
    remove_all(file)
    out = subprocess.check_output([powerloader_binary,
                                   "download",
                                   f"{mock_server}/static/packages/{file['name']}"])

    assert calculate_sha256("xtensor-0.24.0-hc021e02_0.tar.bz2") == file["checksum"]
    assert not Path(file["pdpart_path"]).exists()
    assert Path(file["path"]).exists()
    assert os.path.getsize("xtensor-0.24.0-hc021e02_0.tar.bz2") == file["size"]


# Download from a path that works on the third try
def test_broken_for_three_tries(file, powerloader_binary, mock_server):
    if perform_slow_tests:
        remove_all(file)
        out = subprocess.check_output([powerloader_binary,
                                       "download",
                                       f"{mock_server}/broken_counts/static/packages/{file['name']}"])
        assert calculate_sha256("xtensor-0.24.0-hc021e02_0.tar.bz2") == file["checksum"]
        assert os.path.getsize("xtensor-0.24.0-hc021e02_0.tar.bz2") == file["size"]
    else: pass


# Download from a path that works after some time.
# Warning: this test may take (very) long if "time_thresh" is too small in the server.
def test_broken_temporarily(file, powerloader_binary, mock_server):
    if perform_slow_tests:
        remove_all(file)
        out = subprocess.check_output([powerloader_binary,
                                       "download",
                                       f"{mock_server}/broken_temporary/static/packages/{file['name']}"])
        assert calculate_sha256("xtensor-0.24.0-hc021e02_0.tar.bz2") == file["checksum"]
        assert os.path.getsize("xtensor-0.24.0-hc021e02_0.tar.bz2") == file["size"]
    else: pass


def test_working_download_broken_checksum(file, powerloader_binary, mock_server):
    remove_all(file)

    try:
        out = subprocess.check_output([powerloader_binary,
                                       "download",
                                       f"{mock_server}/static/packages/{file['name']}",
                                       "--sha",
                                       "\"broken_checksum\""])
    except subprocess.CalledProcessError as e: print(e)

    assert not Path(file["pdpart_path"]).exists()
    assert not Path(file["path"]).exists()

# Download a broken file
def test_broken_download_good_checksum(file, powerloader_binary, mock_server):
    remove_all(file)

    try:
        out = subprocess.check_output([powerloader_binary,
                                       "download",
                                       f"{mock_server}/harm_checksum/static/packages/{file['name']}",
                                       "--sha",
                                       "\"broken_checksum\""])
    except subprocess.CalledProcessError as e: print(e)

    assert not Path(file["pdpart_path"]).exists()
    assert not Path(file["path"]).exists()
