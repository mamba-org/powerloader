from fixtures import *


class TestS3Server:
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
    def test_s3_server(self, file, powerloader_binary):
        remove_all(file)
        upload_path = generate_unique_file(file)
        hash_before_upload = calculate_sha256(upload_path)
        up_path = upload_path + ":" + path_to_name(upload_path)
        upload_s3_file(powerloader_binary, up_path, server=file["s3_server"], plain_http=False)
        Path(upload_path).unlink()
        generate_s3_download_yml(file, file["s3_server"], path_to_name(upload_path))
        download_s3_file(powerloader_binary, file)
        assert hash_before_upload == calculate_sha256(upload_path)
