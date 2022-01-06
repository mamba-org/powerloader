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
        upload_path = generate_unique_file(file)
        hash_before_upload = calculate_sha256(upload_path)
        tag, name_on_server = upload_oci(upload_path, powerloader_binary, file["oci_upload_location"])

    def test_download_permanent(self, file, powerloader_binary, checksums):
        Path(get_oci_path(file=file)[1]).unlink(missing_ok=True)
        newpath, tmp_yaml = generate_oci_download_yml(file)
        download_oci_file(powerloader_binary, tmp_yaml, file)
        raise Exception("Stop here!")
        assert checksums[file["name_on_server"]] == calculate_sha256(newpath)

    def set_username(self):
        username = ""
        if self.username_exists():
            username = os.environ.get("GHA_USER")
        else:
            username = "mamba-org"              # GHA_PAT is only available on the main branch
            os.environ["GHA_USER"] = username   # GHA_USER must also be set
        return username

    # Download a file that's always there...
    @pytest.mark.skipif(os.environ.get("GHA_PAT") is None
                        or os.environ.get("GHA_PAT") == "",
                        reason="Environment variable(s) not defined")
    def test_upload_and_download(self, file, powerloader_binary):
        username = self.set_username()
        upload_path = generate_unique_file(file)
        hash_before_upload = calculate_sha256(upload_path)
        tag, name_on_server = upload_oci(upload_path, powerloader_binary, file["oci_upload_location"])
        Path(upload_path).unlink()
        newpath, tmp_yaml = generate_oci_download_yml(file, tag, name_on_server, username)
        download_oci_file(powerloader_binary, tmp_yaml, file)
        assert hash_before_upload == calculate_sha256(newpath)

        # TODO: Delete OCI from server
        # Need to figure out what the package id is
        # Delete: https://stackoverflow.com/questions/59103177/how-to-delete-remove-unlink-unversion-a-package-from-the-github-package-registry
        # https://github.com/actions/delete-package-versions
