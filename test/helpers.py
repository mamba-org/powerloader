from xprocess import ProcessStarter
from urllib.request import urlopen
import sys, socket, pathlib
from pathlib import Path
import math, copy, yaml
import hashlib, os
import json
import glob

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


def filter_broken(file_list, pdp):
    broken = []
    for file in file_list:
        if file.endswith(pdp):
            broken.append(file)
    return broken


def get_prev_headers(mock_server_working):
    with urlopen(f"{mock_server_working}/prev_headers") as fi:
        return json.loads(fi.read().decode('utf-8'))

def get_percentage(delta_size):
    dsize_list = delta_size.decode('utf-8').split(" ")
    of_idx = [i for i, val in enumerate(dsize_list) if val == "of"]
    # print("index(es): " + str(of_idx))
    portion_bytes = 100 * float(dsize_list[of_idx[0] - 1]) / float(dsize_list[of_idx[0] + 1])
    portion_chunks = 100 * float(dsize_list[of_idx[1] - 1]) / float(dsize_list[of_idx[1] + 1])
    #print(dsize_list[of_idx - 1])
    #print(dsize_list[of_idx + 1])
    return portion_bytes, portion_chunks, dsize_list[of_idx[1] + 1]
