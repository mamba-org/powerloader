import platform, glob, datetime, hashlib, subprocess
import shutil, yaml, copy, math
from xprocess import ProcessStarter
from urllib.request import urlopen
import os, subprocess, shutil
import sys, socket, pathlib
from pathlib import Path
import json, os
import requests


def mock_server(xprocess, name, port, pkgs, error_type, uname=None, pwd=None):
    curdir = pathlib.Path(__file__).parent
    print("Starting mock_server")
    authenticate = (uname is not None) and (pwd is not None)

    class Starter(ProcessStarter):

        pattern = "Server started!"
        terminate_on_interrupt = True

        args = [
            sys.executable,
            "-u",
            curdir / "server.py",
            "-p",
            str(port),
            "-e",
            error_type,
            "--pkgs",
            pkgs,
        ]

        if authenticate:
            args.extend(["-u", uname, "--pwd", pwd])

        def startup_check(self):
            s = socket.socket()
            address = "localhost"
            error = False
            try:
                s.connect((address, port))
            except Exception as e:
                print(
                    "something's wrong with %s:%d. Exception is %s" % (address, port, e)
                )
                error = True
            finally:
                s.close()

            return not error

    logfile = xprocess.ensure(name, Starter)

    if authenticate:
        yield f"http://{uname}:{pwd}@localhost:{port}"
    else:
        yield f"http://localhost:{port}"

    xprocess.getinfo(name).terminate()


def generate_random_file(path, size):
    with open(path, "wb") as fout:
        fout.write(os.urandom(size))


def get_pkgs(port, checksums, num_servers=3):
    files = list(checksums.keys())
    section, increment = port % num_servers, len(files) / num_servers
    lb = math.floor(section * increment)
    ub = math.ceil((section + 1) * increment)
    lb = max(lb, 0)
    ub = min(ub, len(files) - 1)
    return set(files[lb:ub])


def yml_content(path):
    with open(path, "r") as stream:
        try:
            return yaml.safe_load(stream)
        except yaml.YAMLError as exc:
            print(exc)


def add_names(file, target):
    yml_cont = yml_content(file[target])
    names = []
    for target in yml_cont["targets"]:
        names.append(Path(target.split("/")[-1]))
    content = copy.deepcopy(yml_cont)
    content["names"] = names
    return content


def path_to_name(path):
    return Path(path).name


def ifnone(var):
    return (var == None) or (var == "")


def get_files(file):
    return glob.glob(str(file["tmp_path"]) + "/*")


def remove_all(file):
    Path(file["output_path"]).unlink(missing_ok=True)
    Path(file["output_path_pdpart"]).unlink(missing_ok=True)

    for fle in get_files(file):
        (file["tmp_path"] / Path(fle)).unlink()


def calculate_sha256(file):
    with open(file, "rb") as f:
        b = f.read()
        readable_hash = hashlib.sha256(b).hexdigest()
        return readable_hash


def unique_filename(with_txt=False):
    if with_txt == False:
        return Path(str(platform.system()).lower().replace("_", "") + "test")
    else:
        return Path(str(platform.system()).lower().replace("_", "") + "test.txt")


def generate_unique_file(file, with_txt=False):
    upload_path = str(file["tmp_path"] / unique_filename(with_txt))
    with open(upload_path, "w+") as f:
        f.write("Content: " + str(datetime.datetime.now()))
    f.close()
    return upload_path


def filter_broken(file_list, pdp):
    broken = []
    for file in file_list:
        if file.endswith(pdp):
            broken.append(file)
    return broken


def gha_credentials_exist():
    user = not (
        (os.environ.get("GHA_USER") is None) or (os.environ.get("GHA_USER") == "")
    )
    pwd = not ((os.environ.get("GHA_PAT") is None) or (os.environ.get("GHA_PAT") == ""))
    return user and pwd


def upload_s3_file(powerloader_binary, up_path, server, plain_http=False):
    command = [powerloader_binary, "upload", up_path]
    if plain_http != False:
        command.extend(["-k", "--plain-http"])
    command.append("-m")
    command.append(server)
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate()
    assert err == "".encode("utf-8")
    assert proc.returncode == 0


def oci_path_resolver(file, tag=None, name_on_server=None, username=None):
    if tag == None:
        t = file["tag"]
    else:
        t = tag

    if name_on_server == None:
        nos = file["name_on_server"]
    else:
        nos = name_on_server

    if username == None:
        un = file["username"]
    else:
        un = username
    return t, nos, un


def generate_s3_download_yml(file, server, filename):
    aws_template = yml_content(file["s3_yml_template"])
    aws_template["targets"] = [
        aws_template["targets"][0].replace("__filename__", filename)
    ]
    aws_template["mirrors"]["s3test"][0]["url"] = aws_template["mirrors"]["s3test"][0][
        "url"
    ].replace("__server__", server)

    with open(str(file["tmp_yml"]), "w") as outfile:
        yaml.dump(aws_template, outfile, default_flow_style=False)


def download_s3_file(powerloader_binary, file, plain_http=False):
    command = [powerloader_binary, "download", "-f", str(file["tmp_yml"])]
    if plain_http != False:
        command.extend(["-k", "--plain-http"])
    command.extend(["-d", str(file["tmp_path"])])
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate()
    assert err == "".encode("utf-8")
    assert proc.returncode == 0


def get_prev_headers(mock_server_working):
    with urlopen(f"{mock_server_working}/prev_headers") as fi:
        return json.loads(fi.read().decode("utf-8"))


def env_vars_absent():
    user_absent = os.environ.get("GHA_USER") == None or os.environ.get("GHA_USER") == ""
    passwd_absent = os.environ.get("GHA_PAT") == None or os.environ.get("GHA_PAT") == ""

    return user_absent and passwd_absent


def get_header_map(filepath):
    new_header = subprocess.check_output(["zck_read_header", str(filepath)]).decode(
        "utf-8"
    )
    new_header = new_header.splitlines()
    header = {}
    for elem in new_header:
        key, value = elem.split(": ")
        header[key] = value
    return header


def get_zchunk(
    file,
    filepath,
    name,
    powerloader_binary,
    mock_server_working,
    outpath,
    extra_params=[],
):
    headers = get_header_map(str(filepath))

    command = [
        powerloader_binary,
        "download",
        f"{mock_server_working}/" + str(name),
    ]

    command += extra_params
    command += [
        "--zck-header-size",
        headers["Header size"],
        "--zck-header-sha",
        headers["Header checksum"],
        "-o",
        str(outpath),
    ]

    print("command: " + str(command))
    out = subprocess.check_output(command)


def resize_zchunk(powerloader_binary, mock_server_working):
    command = [
        powerloader_binary,
        "download",
        f"{mock_server_working}/add_content",
    ]
    out = subprocess.check_output(command)


def get_percentage(delta_size, header_map):
    map = {}
    dsize_list = delta_size.decode("utf-8").split(" ")
    of_idx = [i for i, val in enumerate(dsize_list) if val == "of"]
    map["percentage to download"] = round(
        100 * float(dsize_list[of_idx[0] - 1]) / float(dsize_list[of_idx[0] + 1])
    )
    map["percentage matched chunks"] = round(
        100 * float(dsize_list[of_idx[1] - 1]) / float(dsize_list[of_idx[1] + 1])
    )

    if False:
        map["header size"] = header_map["Header size"]
        map["data size"] = header_map["Data size"]
    else:
        map["header size / data size"] = round(
            float(header_map["Header size"]) / float(header_map["Data size"]), 5
        )
    return map


def get_zck_percent_delta(path):
    percentage = False
    try:
        delta = subprocess.check_output(
            ["zck_delta_size", str(path), str(path).replace(".zck", "_old.ck")]
        )
        header_map = get_header_map(str(path))
        percentage = get_percentage(delta, header_map)
    except Exception as e:
        print("Exception: " + str(e))
    shutil.copy(str(path), str(path).replace(".zck", "_old.ck"))
    return percentage
