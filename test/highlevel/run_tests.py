import os as os
import sys as sys
import subprocess as sub

# Class based?
# Environment variable to binary: TEST_MAMBA_EXE
# Introduce the tests into: .github/workflows/main.yml

def test_powerloader_exists():
    raise Exception("Todo")

def test_working_download():
    outp = sub.check_output(["../../build/powerloader", "download",
                      "https://beta.mamba.pm/get/conda-forge/osx-arm64/xtensor-0.24.0-hc021e02_0.tar.bz2"])

    # print("test string")
    # out, err = capsys.readouterr()

    # Assert file exists
    # File size, checksum,

def test_broken_checksum():
    raise Exception("Todo")

def test_broken_checksum_zchunk():
    raise Exception("Todo")
