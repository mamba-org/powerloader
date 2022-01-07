from fixtures import *

class TestOCImock:
    @classmethod
    def setup_class(cls):
        pass

    @classmethod
    def teardown_class(cls):
        pass

    # Upload a file
    @pytest.mark.skipif(env_vars_absent() == False, reason="Environment variable(s) should not be defined")
    def test_upload(self, file, powerloader_binary):
        upload_path = generate_unique_file(file)
        hash_before_upload = calculate_sha256(upload_path)
        tag, name_on_server = upload_oci(upload_path, powerloader_binary, file["oci_mock_server"], plain_http=True)

    # Download a file that's always there...
    @pytest.mark.skipif(env_vars_absent() == False, reason="Environment variable(s) should not be defined")
    def test_download_permanent(self, file, powerloader_binary, checksums):
        tag, name_on_server, username = oci_path_resolver(file, username="", name_on_server=file["name_on_mock_server"])
        Path(get_oci_path(file, name_on_server, tag)[1]).unlink(missing_ok=True)
        newpath, tmp_yaml = generate_oci_download_yml(file, tag, name_on_server, username, local=True)
        download_oci_file(powerloader_binary, tmp_yaml, file, plain_http=True)
        assert checksums[file["name_on_mock_server"]] == calculate_sha256(newpath)

    # Upload a file and download it again
    @pytest.mark.skipif(env_vars_absent() == False, reason="Environment variable(s) should not be defined")
    def test_upload_and_download(self, file, powerloader_binary):
        # Upload
        upload_path = generate_unique_file(file)
        hash_before_upload = calculate_sha256(upload_path)
        tag, name_on_server = upload_oci(upload_path, powerloader_binary, file["oci_mock_server"], plain_http=True)
        Path(upload_path).unlink()

        # Download
        newpath, tmp_yaml = generate_oci_download_yml(file, tag, name_on_server, username="", local=True)
        download_oci_file(powerloader_binary, tmp_yaml, file, plain_http=True)
        assert hash_before_upload == calculate_sha256(newpath)

        # TODO: Delete OCI from server
        # Need to figure out what the package id is
        # Delete: https://stackoverflow.com/questions/59103177/how-to-delete-remove-unlink-unversion-a-package-from-the-github-package-registry
        # https://github.com/actions/delete-package-versions
