import platform, glob, datetime, hashlib, subprocess
import shutil, yaml, copy, math
from xprocess import ProcessStarter
from urllib.request import urlopen
import sys, socket, pathlib
from pathlib import Path
import json, os


def mock_server(xprocess, name, port, pkgs, error_type,
                uname=None, pwd=None):
    curdir = pathlib.Path(__file__).parent
    print("Starting mock_server")
    authenticate = (uname is not None) and (pwd is not None)

    class Starter(ProcessStarter):

        pattern = "Server started!"
        terminate_on_interrupt = True

        args = [sys.executable, "-u", curdir / 'server.py',
                '-p', str(port), "-e", error_type,
                "--pkgs", pkgs]

        if authenticate:
            args.extend(["-u", uname, "--pwd", pwd])

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

    if authenticate:
        yield f"http://{uname}:{pwd}@localhost:{port}"  # True
    else:
        yield f"http://localhost:{port}"  # True

    # clean up whole process tree afterwards
    xprocess.getinfo(name).terminate()


def generate_random_file(path, size):
    with open(path, 'wb') as fout:
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
    return str(path).split("/")[-1]


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
        # read entire file as bytes
        b = f.read()
        readable_hash = hashlib.sha256(b).hexdigest();
        return readable_hash


def unique_filename(with_txt=False):
    if with_txt == False:
        return Path(str(platform.system()).lower().replace("_", "") + "test")
    else:
        return Path(str(platform.system()).lower().replace("_", "") + "test.txt")


# Generate a unique file
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


def upload_s3_file(powerloader_binary, up_path, server, plain_http=False):
    command = [powerloader_binary, "upload", up_path]
    if (plain_http != False):
        command.extend(["-k", "--plain-http"])
    command.append("-m")
    command.append(server)
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate()
    assert proc.returncode == 0


def generate_s3_download_yml(file, server, filename):
    aws_template = yml_content(file["s3_yml_template"])
    aws_template["targets"] = \
        [aws_template["targets"][0].replace("__filename__", filename)]
    aws_template["mirrors"]["s3test"][0]["url"] = \
        aws_template["mirrors"]["s3test"][0]["url"].replace("__server__", server)

    with open(str(file["tmp_yml"]), 'w') as outfile:
        yaml.dump(aws_template, outfile, default_flow_style=False)


def download_s3_file(powerloader_binary, file, plain_http=False):
    command = [powerloader_binary, "download", "-f", str(file["tmp_yml"])]
    if (plain_http != False):
        command.extend(["-k", "--plain-http"])
    command.extend(["-d", str(file["tmp_path"])])
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    print("command: " + str(command))
    out, err = proc.communicate()
    assert proc.returncode == 0


def get_prev_headers(mock_server_working):
    with urlopen(f"{mock_server_working}/prev_headers") as fi:
        return json.loads(fi.read().decode('utf-8'))


def get_percentage(delta_size):
    dsize_list = delta_size.decode('utf-8').split(" ")
    of_idx = [i for i, val in enumerate(dsize_list) if val == "of"]
    portion_bytes = 100 * float(dsize_list[of_idx[0] - 1]) / float(dsize_list[of_idx[0] + 1])
    portion_chunks = 100 * float(dsize_list[of_idx[1] - 1]) / float(dsize_list[of_idx[1] + 1])
    return portion_bytes, portion_chunks, dsize_list[of_idx[1] + 1]
