from fixtures import *

class TestS3:
    @classmethod
    def setup_class(cls):
        pass

    @classmethod
    def teardown_class(cls):
        pass

    def s3_mock_keys_set(self):
        self.acc_key, self.sec_key = os.environ['AWS_ACCESS_KEY'], os.environ['AWS_SECRET_KEY']
        os.environ['AWS_ACCESS_KEY'], os.environ['AWS_SECRET_KEY'] = \
            os.environ['AWS_ACCESS_KEY_ID'], os.environ['AWS_SECRET_ACCESS_KEY']

    def s3_mock_keys_reset(self):
        os.environ['AWS_ACCESS_KEY'], os.environ['AWS_SECRET_KEY'] = self.acc_key, self.sec_key

    @pytest.mark.skipif(os.environ.get("AWS_ACCESS_KEY") is None
                        or os.environ.get("AWS_ACCESS_KEY") == ""
                        or os.environ.get("AWS_SECRET_KEY") is None
                        or os.environ.get("AWS_SECRET_KEY") == ""
                        or os.environ.get("GHA_USER") is None
                        or os.environ.get("GHA_USER") == ""
                        or os.environ.get("AWS_DEFAULT_REGION") is None
                        or os.environ.get("AWS_DEFAULT_REGION") == "",
                        reason="Environment variable(s) not defined")
    def test_yml_s3_mirror(self, file, checksums, powerloader_binary):
        self.s3_mock_keys_set()
        remove_all(file)

        # Generate a YML file for the download
        filename = str(file["s3_bucketname"]) + "/" + path_to_name(file["xtensor_path"])
        generate_s3_download_yml(file, file["s3_mock_server"], filename)

        out = subprocess.check_output([powerloader_binary, "download",
                                       "-f", file["tmp_yml"], "--plain-http",
                                       "-d", file["tmp_path"]])

        Path(file["tmp_yml"]).unlink()

        for fp in get_files(file):
            assert calculate_sha256(fp) == checksums[str(path_to_name(fp))]
        self.s3_mock_keys_reset()

    @pytest.mark.skipif(os.environ.get("AWS_ACCESS_KEY") is None
                        or os.environ.get("AWS_ACCESS_KEY") == ""
                        or os.environ.get("AWS_SECRET_KEY") is None
                        or os.environ.get("AWS_SECRET_KEY") == ""
                        or os.environ.get("GHA_USER") is None
                        or os.environ.get("GHA_USER") == ""
                        or os.environ.get("AWS_ACCESS_KEY_ID") is None
                        or os.environ.get("AWS_ACCESS_KEY_ID") == ""
                        or os.environ.get("AWS_SECRET_ACCESS_KEY") is None
                        or os.environ.get("AWS_SECRET_ACCESS_KEY") == ""
                        or os.environ.get("AWS_DEFAULT_REGION") is None
                        or os.environ.get("AWS_DEFAULT_REGION") == "",
                        reason="Environment variable(s) not defined")
    def test_s3_mock(self, file, powerloader_binary):
        self.s3_mock_keys_set()
        remove_all(file)
        upload_path = generate_unique_file(file)

        # Store the checksum for later
        hash_before_upload = calculate_sha256(upload_path)

        # Upload the file
        up_path = upload_path + ":" + str(file["s3_bucketname"] / Path(path_to_name(upload_path)))
        upload_s3_file(powerloader_binary, up_path, server=file["s3_mock_server"], plain_http=True)

        # Delete the file
        Path(upload_path).unlink()

        # Generate a YML file for the download
        filename = str(file["s3_bucketname"]) + "/" + path_to_name(upload_path)
        generate_s3_download_yml(file, file["s3_mock_server"], filename)

        # Download using this YML file
        download_s3_file(powerloader_binary, file, plain_http=True)

        # Check that the downloaded file is the same as the uploaded file
        assert hash_before_upload == calculate_sha256(upload_path)

        self.s3_mock_keys_reset()

    @pytest.mark.skipif(os.environ.get("AWS_ACCESS_KEY") is None
                        or os.environ.get("AWS_ACCESS_KEY") == ""
                        or os.environ.get("AWS_SECRET_KEY") is None
                        or os.environ.get("AWS_SECRET_KEY") == ""
                        or os.environ.get("GHA_USER") is None
                        or os.environ.get("GHA_USER") == ""
                        or os.environ.get("AWS_DEFAULT_REGION") is None
                        or os.environ.get("AWS_DEFAULT_REGION") == "",
                        reason="Environment variable(s) not defined")
    def test_s3_server(self, file, powerloader_binary):
        remove_all(file)
        upload_path = generate_unique_file(file)

        # Store the checksum for later
        hash_before_upload = calculate_sha256(upload_path)

        # Upload the file
        up_path = upload_path + ":" + path_to_name(upload_path)
        upload_s3_file(powerloader_binary, up_path, server=file["s3_server"], plain_http=False)

        # Delete the file
        Path(upload_path).unlink()

        # Generate a YML file for the download
        generate_s3_download_yml(file, file["s3_server"], path_to_name(upload_path))

        # Download using this YML file
        download_s3_file(powerloader_binary, file)

        # Check that the downloaded file is the same as the uploaded file
        assert hash_before_upload == calculate_sha256(upload_path)

    @pytest.mark.skipif(os.environ.get("AWS_ACCESS_KEY") is None
                        or os.environ.get("AWS_ACCESS_KEY") == ""
                        or os.environ.get("AWS_SECRET_KEY") is None
                        or os.environ.get("AWS_SECRET_KEY") == ""
                        or os.environ.get("GHA_USER") is None
                        or os.environ.get("GHA_USER") == ""
                        or os.environ.get("AWS_ACCESS_KEY_ID") is None
                        or os.environ.get("AWS_ACCESS_KEY_ID") == ""
                        or os.environ.get("AWS_SECRET_ACCESS_KEY") is None
                        or os.environ.get("AWS_SECRET_ACCESS_KEY") == ""
                        or os.environ.get("AWS_DEFAULT_REGION") is None
                        or os.environ.get("AWS_DEFAULT_REGION") == "",
                        reason="Environment variable(s) not defined")
    def test_s3_mock_mod_txt(self, file, powerloader_binary):
        self.s3_mock_keys_set()
        remove_all(file)
        upload_path = generate_unique_file(file, with_txt=True)

        # Store the checksum for later
        hash_before_upload = calculate_sha256(upload_path)

        # Upload the file
        up_path = upload_path + ":" + str(file["s3_bucketname"] / Path(path_to_name(upload_path)))
        upload_s3_file(powerloader_binary, up_path, server=file["s3_mock_server"], plain_http=True)

        # Delete the file
        Path(upload_path).unlink()

        # Generate a YML file for the download
        filename = str(file["s3_bucketname"]) + "/" + path_to_name(upload_path)
        generate_s3_download_yml(file, file["s3_mock_server"], filename)

        # Download using this YML file
        download_s3_file(powerloader_binary, file, plain_http=True)

        # Check that the downloaded file is the same as the uploaded file
        hash_after_upload = calculate_sha256(upload_path)
        assert hash_before_upload == hash_after_upload

        self.s3_mock_keys_reset()

    @pytest.mark.skipif(os.environ.get("AWS_ACCESS_KEY") is None
                        or os.environ.get("AWS_ACCESS_KEY") == ""
                        or os.environ.get("AWS_SECRET_KEY") is None
                        or os.environ.get("AWS_SECRET_KEY") == ""
                        or os.environ.get("GHA_USER") is None
                        or os.environ.get("GHA_USER") == ""
                        or os.environ.get("AWS_ACCESS_KEY_ID") is None
                        or os.environ.get("AWS_ACCESS_KEY_ID") == ""
                        or os.environ.get("AWS_SECRET_ACCESS_KEY") is None
                        or os.environ.get("AWS_SECRET_ACCESS_KEY") == ""
                        or os.environ.get("AWS_DEFAULT_REGION") is None
                        or os.environ.get("AWS_DEFAULT_REGION") == "",
                        reason="Environment variable(s) not defined")
    def test_s3_mock_yml_mod_loc(self, file, powerloader_binary):
        self.s3_mock_keys_set()
        remove_all(file)
        upload_path = generate_unique_file(file)

        # Store the checksum for later
        hash_before_upload = calculate_sha256(upload_path)

        # Upload the file
        up_path = upload_path + ":" + str(file["s3_bucketname"] / Path(path_to_name(upload_path)))
        upload_s3_file(powerloader_binary, up_path, server=file["s3_mock_server"], plain_http=True)

        # Delete the file
        Path(upload_path).unlink()

        # Generate a YML file for the download
        server = file["s3_mock_server"] + "/" + str(file["s3_bucketname"])
        generate_s3_download_yml(file, server, path_to_name(upload_path))

        # Download using this YML file
        download_s3_file(powerloader_binary, file, plain_http=True)

        # Check that the downloaded file is the same as the uploaded file
        assert hash_before_upload == calculate_sha256(upload_path)

        self.s3_mock_keys_reset()
