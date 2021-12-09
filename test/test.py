import sys, socket, pytest, py, pathlib
from xprocess import ProcessStarter
from pathlib import Path
import subprocess
import platform
import os as os
import hashlib
import time

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
    return file_map


# @pytest.fixture
# def mock_server(xprocess):
#     port = 5777
#     curdir = pathlib.Path(__file__).parent

#     class Starter(ProcessStarter):
#         # startup pattern
#         pattern = "Server started!"

#         terminate_on_interrupt = True

#         # command to start process
#         args = ['python', str(curdir / 'server.py'), '-p', str(port)]

#     # ensure process is running and return its logfile
#     logfile = xprocess.ensure("mock_server", Starter)

#     yield f"http://localhost:{port}" # True

#     # clean up whole process tree afterwards
#     xprocess.getinfo("mock_server").terminate()


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

@pytest.fixture
def server_address():
    return "http://localhost:5555"

def test_working_download(file, powerloader_binary, server_address):
    remove_file(file["path"])
    remove_file(file["pdpart_path"])

    # Slow because of the download
    out = subprocess.check_output([powerloader_binary,
                                   "download",
                                   f"{server_address}/static/packages/{file['name']}"])


    fixed = False
    if fixed:
        assert not Path(file["pdpart_path"]).exists()
        assert Path(file["path"]).exists()
        assert calculate_sha256("xtensor-0.24.0-hc021e02_0.tar.bz2") == "c318afd7058e4721d2a8f95556e7290245c89d566491a3fe4f5e618c6b50d590"
        assert os.path.getsize("xtensor-0.24.0-hc021e02_0.tar.bz2") == 1843

    else:
        assert Path(file["pdpart_path"]).exists()
        assert not Path(file["path"]).exists()
        assert calculate_sha256("xtensor-0.24.0-hc021e02_0.tar.bz2.pdpart") == "c318afd7058e4721d2a8f95556e7290245c89d566491a3fe4f5e618c6b50d590"
        assert os.path.getsize("xtensor-0.24.0-hc021e02_0.tar.bz2.pdpart") == 1843


    remove_file(file["path"])
    remove_file(file["pdpart_path"])

    assert not Path(file["pdpart_path"]).exists()
    assert not Path(file["path"]).exists()
