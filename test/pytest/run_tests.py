from pathlib import Path
import subprocess
import platform
import os as os
import pytest


@pytest.fixture
def get_proj_root(cwd=os.getcwd()):
    if os.getenv("TEST_ROOT"):
        proj_root = os.getenv("TEST_ROOT")
    else:
        proj_root = cwd
    if not Path(proj_root).exists():
        print("POWERLOADER NOT FOUND!")
    return proj_root


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


def remove_file(file_path):
    args = ("rm", "-rf", file_path)
    subprocess.call('%s %s %s' % args, shell=True)
    assert not Path(file_path).exists()


def test_working_download(file):
    remove_file(file["path"])
    remove_file(file["pdpart_path"])

    # Slow because of the download
    out = subprocess.check_output(["build/powerloader", "download", file["url"]])
    assert not Path(file["pdpart_path"]).exists()
    assert Path(file["path"]).exists()
