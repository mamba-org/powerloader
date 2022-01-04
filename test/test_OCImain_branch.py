from fixtures import *

"""
    These tests must only be run when on the main branch

    def get_git_branch(self, file):
        repo = Repository(file["location"])
        head = repo.lookup_reference('HEAD').resolve()
        head = str(head.name).split("/")[-1]
        return head

        get_git_branch(file) == "main"

"""
class TestOCIServer:
    def username_exists(self):
        return not ((os.environ.get("GHA_USER") is None) or (os.environ.get("GHA_USER") == ""))

    @classmethod
    def setup_class(cls):
        pass

    @classmethod
    def teardown_class(cls):
        pass

    def test_upload(self, file, powerloader_binary):
        # Generate a unique file
        upload_path = generate_unique_file(file)

        # Store the checksum for later
        hash_before_upload = calculate_sha256(upload_path)
        print("hash before upload: " + str(hash_before_upload))

        # Upload the file
        tag = "321"
        name_on_server = path_to_name(upload_path)
        command = [powerloader_binary, "upload", upload_path + ":"
                    + name_on_server + ":" + tag, "-m", file["oci_upload_location"]]
        proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = proc.communicate()
        print("command: " + str(command))
        print("out: " + str(out))
        print("err: " + str(err))
        assert proc.returncode == 0


    def test_download_permanent(self):
        pass

    def test_upload_and_download(self):
        pass

    # Download a file that's always there...
    @pytest.mark.skipif(os.environ.get("GHA_PAT") is None
                        or os.environ.get("GHA_PAT") == "",
                        reason="Environment variable(s) not defined")
    def test_oci_fixes(self, file, powerloader_binary):
        username = ""
        if self.username_exists():
            username = os.environ.get("GHA_USER")
        else:
            username = "mamba-org"              # GHA_PAT is only available on the main branch
            os.environ["GHA_USER"] = username   # GHA_USER must also be set
        self.username = username

        # Generate a unique file
        upload_path = generate_unique_file(file)

        # Store the checksum for later
        hash_before_upload = calculate_sha256(upload_path)

        # Upload the file
        tag = "321"
        name_on_server = path_to_name(upload_path)
        command = [powerloader_binary, "upload", upload_path + ":"
                    + name_on_server + ":" + tag, "-m", file["oci_upload_location"]]
        proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = proc.communicate()
        assert proc.returncode == 0

        # Delete the file locally
        Path(upload_path).unlink()

        # Generate yaml file
        oci_template = yml_content(file["oci_template"])
        newname = name_on_server + "-" + tag
        newpath = file["tmp_path"] / Path(newname)
        oci_template["targets"][0] = oci_template["targets"][0].replace("__filename__", newname)

        oci_template["mirrors"]["ocitest"][0] = oci_template["mirrors"]["ocitest"][0].replace("__username__", username)

        tmp_yaml = file["tmp_path"] / Path("tmp.yml")
        with open(str(tmp_yaml), 'w') as outfile:
            yaml.dump(oci_template, outfile, default_flow_style=False)

        # Download using this YML file
        proc = subprocess.Popen([powerloader_binary, "download",
                                    "-f", str(tmp_yaml),
                                    "-d", str(file["tmp_path"])],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = proc.communicate()
        assert proc.returncode == 0

        # Check that the downloaded file is the same as the uploaded file
        assert hash_before_upload == calculate_sha256(newpath)

    # TODO: Delete OCI from server
    # Need to figure out what the package id is
    # Delete: https://stackoverflow.com/questions/59103177/how-to-delete-remove-unlink-unversion-a-package-from-the-github-package-registry
    # https://github.com/actions/delete-package-versions
