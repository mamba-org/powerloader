from fixtures import *

class TestS3Mock:

    def s3_mock_keys_set(self):
        """
        This is a temporary workaround due to issues setting a custom password in minio
        """
        self.acc_key, self.sec_key = os.environ['AWS_ACCESS_KEY_ID'], os.environ['AWS_SECRET_ACCESS_KEY']
        os.environ['AWS_ACCESS_KEY_ID'], os.environ['AWS_SECRET_ACCESS_KEY'] = "minioadmin", "minioadmin"

    def s3_mock_keys_reset(self):
        """
        This is a temporary workaround due to issues setting a custom password in minio
        """
        os.environ['AWS_ACCESS_KEY_ID'], os.environ['AWS_SECRET_ACCESS_KEY'] = self.acc_key, self.sec_key

    @classmethod
    def setup_class(cls):
        pass

    @classmethod
    def teardown_class(cls):
        pass



    @pytest.mark.skipif(ifnone("AWS_SECRET_ACCESS_KEY")
                        or ifnone("AWS_ACCESS_KEY_ID")
                        or ifnone("GHA_USER")
                        or ifnone("AWS_DEFAULT_REGION"),
                        reason="Environment variable(s) not defined")
    def test_s3_mock(self, file, powerloader_binary):
        # self.s3_mock_keys_set()
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
        # self.s3_mock_keys_reset()


    @pytest.mark.skipif(ifnone("AWS_SECRET_ACCESS_KEY")
                        or ifnone("AWS_ACCESS_KEY_ID")
                        or ifnone("GHA_USER")
                        or ifnone("AWS_DEFAULT_REGION"),
                        reason="Environment variable(s) not defined")
    def test_s3_mock_mod_txt(self, file, powerloader_binary):
        # self.s3_mock_keys_set()
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
        # self.s3_mock_keys_reset()


    @pytest.mark.skipif(ifnone("AWS_SECRET_ACCESS_KEY")
                        or ifnone("AWS_ACCESS_KEY_ID")
                        or ifnone("GHA_USER")
                        or ifnone("AWS_DEFAULT_REGION"),
                        reason="Environment variable(s) not defined")
    def test_s3_mock_yml_mod_loc(self, file, powerloader_binary):
        # self.s3_mock_keys_set()
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
        # self.s3_mock_keys_reset()

    @pytest.mark.skipif(ifnone("AWS_SECRET_ACCESS_KEY")
                        or ifnone("AWS_ACCESS_KEY_ID")
                        or ifnone("GHA_USER")
                        or ifnone("AWS_DEFAULT_REGION"),
                        reason="Environment variable(s) not defined")
    def test_yml_s3_mock_mirror(self, file, checksums, powerloader_binary):
        # self.s3_mock_keys_set()
        remove_all(file)

        # Generate a YML file for the download
        filename = str(file["s3_bucketname"]) + "/" + path_to_name(file["xtensor_path"])
        generate_s3_download_yml(file, file["s3_mock_server"], filename)

        # Download using this YML file
        download_s3_file(powerloader_binary, file, plain_http=True)

        Path(file["tmp_yml"]).unlink()

        for fp in get_files(file):
            assert calculate_sha256(fp) == checksums[str(path_to_name(fp))]
        # self.s3_mock_keys_reset()
