from codecs import ignore_errors
import platform, glob, datetime, subprocess
import shutil, yaml, copy, math
from xprocess import ProcessStarter
from urllib.request import urlopen
import os, subprocess, shutil
import sys, socket, pathlib
from growing_file import *
import numpy as np
import json, os, pytest
import requests
import time
from pathlib import Path
import hashlib


def mock_server(
    xprocess,
    name,
    port,
    pkgs,
    error_type,
    uname=None,
    pwd=None,
    content_path=None,
    outfile=None,
):
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
            "--cpath",
            content_path,
            "--outfile",
            outfile,
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
    return (var is None) or (var == "")


def get_files(file, expect=None):
    paths = glob.glob(str(file["tmp_path"]) + "/*")
    if expect == "zero":
        assert len(paths) == 0
    elif expect == ">zero":
        assert len(paths) > 0
    else:
        pass
    return paths


def remove_all(file):
    Path(file["output_path"]).unlink(missing_ok=True)
    Path(file["output_path_pdpart"]).unlink(missing_ok=True)

    for fle in get_files(file):
        fp = file["tmp_path"] / Path(fle)
        if fp.is_dir() == True:
            shutil.rmtree(fp)
        else:
            fp.unlink()


def generate_unique_file(file, filename):
    upload_path = str(file["tmp_path"] / filename)
    with open(upload_path, "w+") as f:
        f.write("Content: " + str(datetime.datetime.now()))
    f.close()
    assert Path(upload_path).exists()
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


def gha_credentials_dont_exist():
    user_not_set = (os.environ.get("GHA_USER") is None) or (
        os.environ.get("GHA_USER") == ""
    )
    pwd_not_set = (os.environ.get("GHA_PAT") is None) or (
        os.environ.get("GHA_PAT") == ""
    )
    return user_not_set and pwd_not_set


def aws_credential_missing():
    sec_acc_key = ifnone(os.environ.get("AWS_SECRET_ACCESS_KEY"))
    acc_key_id = ifnone(os.environ.get("AWS_ACCESS_KEY_ID"))
    default_region = ifnone(os.environ.get("AWS_DEFAULT_REGION"))
    return sec_acc_key or acc_key_id or default_region


def aws_credential_all_missing():
    sec_acc_key = ifnone(os.environ.get("AWS_SECRET_ACCESS_KEY"))
    acc_key_id = ifnone(os.environ.get("AWS_ACCESS_KEY_ID"))
    default_region = ifnone(os.environ.get("AWS_DEFAULT_REGION"))
    return sec_acc_key and acc_key_id and default_region


def aws_credentials_all_exist():
    sec_acc_key = ifnone(os.environ.get("AWS_SECRET_ACCESS_KEY"))
    acc_key_id = ifnone(os.environ.get("AWS_ACCESS_KEY_ID"))
    default_region = ifnone(os.environ.get("AWS_DEFAULT_REGION"))
    return not sec_acc_key and not acc_key_id and not default_region


skip_aws_credentials = pytest.mark.skipif(
    aws_credential_missing(), reason="Not all AWS credentials are set"
)


def run_command(command, ok_in=False):
    failed = True
    try:
        proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        out, err = proc.communicate()

        try:
            ok = "BucketAlreadyOwnedByYou" in err.decode("utf-8")
            if ok_in != False:
                ok = ok or (ok_in in str(out))
            assert (err == "".encode("utf-8")) or ok
            assert "error" not in str(out.decode("utf-8"))
            assert (proc.returncode == 0) or ok
            failed = False
        except Exception as e:
            print("command: " + str(command))
            print("out: " + str(out))
            print("err: " + str(err))
            print("returncode: " + str(proc.returncode))
            print("Inner Exception: " + str(e))
        return out.decode("utf-8")
    except Exception as e2:
        print("command: " + str(command))
        print("Outer Exception: " + str(e2))
    if failed == True:
        raise Exception("Command failed!")
    return ""


def upload_s3_file(powerloader_binary, up_path, server, plain_http=False):
    command = [powerloader_binary, "upload", up_path]
    if plain_http != False:
        command.extend(["-k", "--plain-http"])
    command.append("-m")
    command.append(server)
    run_command(command)


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
    assert Path(file["tmp_yml"]).exists()


def download_s3_file(powerloader_binary, file, plain_http=False, verbose=False):
    command = [powerloader_binary, "download", "-f", str(file["tmp_yml"])]
    if plain_http != False:
        command.extend(["-k", "--plain-http"])
    if verbose != False:
        command.extend(["-v"])
    command.extend(["-d", str(file["tmp_path"])])
    run_command(command)
    assert Path(file["tmp_yml"]).exists()


def get_prev_headers(mock_server_working, n_headers=1):
    print("urlopen: " + str(f"{mock_server_working}/prev_headers"))
    with urlopen(f"{mock_server_working}/prev_headers") as fi:
        x = json.loads(fi.read().decode("utf-8"))
        print("json: " + str(x))
        if not x:
            return x
        if n_headers == 1:
            return x[-1]
        else:
            return x[-n_headers:]


def clear_prev_headers(mock_server_working):
    urlopen(f"{mock_server_working}/clear_prev_headers")


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


# Unzck doesn't allow for the output path to be specified yet.
def unzck(compressed_file, ghcr_tag=None):
    decompressed_file = Path(str(compressed_file).replace(".zck", ""))
    original_dir = os.getcwd()
    parent = compressed_file.parent.resolve()
    os.chdir(parent)
    if ghcr_tag != None:
        new_compressed_file = Path(str(compressed_file).replace("-" + ghcr_tag, ""))
        decompressed_file = Path(str(new_compressed_file).replace(".zck", ""))
        shutil.copy(compressed_file, new_compressed_file)
        run_command(["unzck", str(new_compressed_file.name)])
    else:
        run_command(["unzck", str(compressed_file.name)])
    os.chdir(original_dir)
    assert decompressed_file.exists()


# TODO: Download to "localpath" when powerloader supports this
def get_zchunk_s3(file, filepath, powerloader_binary, localpath, mock=True):
    # TODO: Check headers
    # headers = get_header_map(str(filepath))

    if mock:
        generate_s3_download_yml(
            file, file["s3_mock_server"], str(file["s3_bucketname"] / filepath.name)
        )
        plain_http = True

    else:
        generate_s3_download_yml(file, file["s3_server"], str(filepath.name))
        plain_http = False

    download_s3_file(powerloader_binary, file, plain_http)
    unzck(file["tmp_path"] / filepath.name)


def get_zchunk_regular(
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

    run_command(command)


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

    map["header size"] = round(float(header_map["Header size"]), 5)
    return map


def get_zck_percent_delta(path, first_time):
    failed = True
    percentage = False
    try:
        if first_time == False:
            delta = subprocess.check_output(
                ["zck_delta_size", str(path), str(path).replace(".zck", "_old.zck")]
            )

            header_map = get_header_map(str(path))
            percentage = get_percentage(delta, header_map)

        failed = False
    except Exception as e:
        print("Exception: " + str(e))
    shutil.copy(str(path), str(path).replace(".zck", "_old.zck"))
    if failed:
        raise Exception("Failed to get_zck_percent_delta")
    return percentage


def calculate_sha256(file):
    assert Path(file).exists()
    with open(file, "rb") as f:
        b = f.read()
        readable_hash = hashlib.sha256(b).hexdigest()
        return readable_hash


def generate_content(file, checksums):
    np.random.seed(seed=42)
    content = bytes(np.random.randint(256, size=2 ** 26))
    if hashlib.sha256(content).hexdigest() != checksums["random"]:
        raise Exception("Content must always be the same for deterministic tests")
    content_path = str(file["tmp_path"] / Path("original_content"))
    # Content can't be passed to the server as a parameter (too verbose)
    with open(content_path, "wb") as fout:
        fout.write(content)
    assert Path(content_path).exists()
    assert calculate_sha256(content_path) == checksums["random"]
    return content_path


def path_filter(fp):
    ignore = [".gitignore", "lorem.txt"]
    proceed = os.path.isfile(fp) or os.path.islink(fp)
    for substring in ignore:
        keep = substring in fp
        proceed &= not keep
    return proceed


def delete_content(folder):
    if os.path.exists(folder):
        for file in os.listdir(folder):
            fp = os.path.join(folder, file)
            try:
                if path_filter(fp):
                    os.unlink(fp)
                elif os.path.isdir(fp):
                    shutil.rmtree(fp)
            except Exception as e:
                print("Exception: " + str(e))


def set_env_var(file, name):
    os.environ[name] = file[name]


def setup_file(content_path, filename):
    content_present = True
    if content_path != "" and content_path != None and content_path != "None":
        name = "gf" + filename + ".zck"
        filepath = Path(os.path.dirname(os.path.abspath(__file__)))
        # if s3:
        filepath = filepath / Path("conda_mock")
        filepath = filepath / Path("static/zchunk/growing_file/") / Path(name)

        gf = GrowingFile(path=filepath, content_path=content_path, initial_exponent=10)

        return content_present, gf
    else:
        content_present = False

    return content_present, None


def aws_cp(file, localpath, upname):
    upload_to_testbucket = [
        "aws",
        "--endpoint-url",
        str(file["s3_mock_endpoint"]),
        "s3",
        "cp",
        str(localpath),
        str(upname),
    ]
    out = run_command(upload_to_testbucket)

    check_testbucket = [
        "aws",
        "--endpoint-url",
        file["s3_mock_endpoint"],
        "s3",
        "ls",
        file["s3_bucketname"],
    ]
    out = run_command(check_testbucket)
    assert str(localpath.name) in str(out)
