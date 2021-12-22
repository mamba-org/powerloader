import platform, glob, datetime, hashlib, subprocess
import shutil, yaml, copy, math
from pathlib import Path


def yml_content(path):
    with open(path, "r") as stream:
        try:
            return yaml.safe_load(stream)
        except yaml.YAMLError as exc:
            print(exc)


def path_to_name(path):
    return str(path).split("/")[-1]


def calculate_sha256(file):
    with open(file, "rb") as f:
        # read entire file as bytes
        b = f.read()
        readable_hash = hashlib.sha256(b).hexdigest();
        return readable_hash


def get_files(file):
    return glob.glob(str(file["tmp_path"]) + "/*")


def remove_all(file):
    Path(file["output_path"]).unlink(missing_ok=True)
    Path(file["output_path_pdpart"]).unlink(missing_ok=True)

    for fle in get_files(file):
        (file["tmp_path"] / Path(fle)).unlink()


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
    out, err = proc.communicate()
    assert proc.returncode == 0
