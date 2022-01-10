import sys, socket, pytest, py, pathlib
from xprocess import ProcessStarter
from urllib.request import urlopen
import platform, datetime
import shutil, subprocess
import os, time, json

from helpers import *

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
    file_map["s3_server"] = "s3://powerloadertestbucket.s3.eu-central-1.amazonaws.com"
    file_map["s3_mock_server"] = "s3://127.0.0.1:9000"
    file_map["s3_yml_template"] = file_map["test_path"] / Path("s3template.yml")
    file_map["s3_bucketname"] = Path("testbucket")
    file_map["tmp_yml"] = file_map["tmp_path"] / Path("tmp.yml")
    file_map["xtensor_path"] = file_map["test_path"] / \
                               Path("conda_mock/static/packages/xtensor-0.23.9-hc021e02_1.tar.bz2")
    file_map["oci_template"] = file_map["test_path"] / Path("ocitemplate.yml")
    file_map["oci_upload_location"] = "oci://ghcr.io"
    file_map["oci_mock_server"] = "oci://localhost:5000"
    file_map["name_on_mock_server"] = "mock_artifact"
    file_map["name_on_server"] = "artifact"
    file_map["tag"] = "1.0"
    file_map["username"] = "wolfv"

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
    cksums["artifact"] = "c5be3ea75353851e1fcf3a298af3b6cfd2af3d7ff018ce52657b6dbd8f986aa4"
    cksums["mock_artifact"] = "5b3513f580c8397212ff2c8f459c199efc0c90e4354a5f3533adf0a3fff3a530"
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
