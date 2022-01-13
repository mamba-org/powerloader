from fixtures import *


class TestS3Mock:
    @pytest.mark.skipif(
        ifnone("AWS_SECRET_ACCESS_KEY")
        or ifnone("AWS_ACCESS_KEY_ID")
        or ifnone("AWS_DEFAULT_REGION"),
        reason="Environment variable(s) not defined",
    )
    def test_s3_mock(self, file, powerloader_binary):
        remove_all(file)
        upload_path = generate_unique_file(file)
        hash_before_upload = calculate_sha256(upload_path)
        up_path = (
            upload_path
            + ":"
            + str(file["s3_bucketname"] / Path(path_to_name(upload_path)))
        )
        upload_s3_file(
            powerloader_binary, up_path, server=file["s3_mock_server"], plain_http=True
        )

        Path(upload_path).unlink()
        filename = str(file["s3_bucketname"]) + "/" + path_to_name(upload_path)
        generate_s3_download_yml(file, file["s3_mock_server"], filename)
        download_s3_file(powerloader_binary, file, plain_http=True)
        assert hash_before_upload == calculate_sha256(upload_path)

    @pytest.mark.skipif(
        ifnone("AWS_SECRET_ACCESS_KEY")
        or ifnone("AWS_ACCESS_KEY_ID")
        or ifnone("AWS_DEFAULT_REGION"),
        reason="Environment variable(s) not defined",
    )
    def test_s3_mock_mod_txt(self, file, powerloader_binary):
        remove_all(file)
        upload_path = generate_unique_file(file, with_txt=True)
        hash_before_upload = calculate_sha256(upload_path)
        up_path = (
            upload_path
            + ":"
            + str(file["s3_bucketname"] / Path(path_to_name(upload_path)))
        )
        upload_s3_file(
            powerloader_binary, up_path, server=file["s3_mock_server"], plain_http=True
        )
        Path(upload_path).unlink()
        filename = str(file["s3_bucketname"]) + "/" + path_to_name(upload_path)
        generate_s3_download_yml(file, file["s3_mock_server"], filename)
        download_s3_file(powerloader_binary, file, plain_http=True)
        hash_after_upload = calculate_sha256(upload_path)
        assert hash_before_upload == hash_after_upload

    @pytest.mark.skipif(
        ifnone("AWS_SECRET_ACCESS_KEY")
        or ifnone("AWS_ACCESS_KEY_ID")
        or ifnone("AWS_DEFAULT_REGION"),
        reason="Environment variable(s) not defined",
    )
    def test_s3_mock_yml_mod_loc(self, file, powerloader_binary):
        remove_all(file)
        upload_path = generate_unique_file(file)
        hash_before_upload = calculate_sha256(upload_path)
        up_path = (
            upload_path
            + ":"
            + str(file["s3_bucketname"] / Path(path_to_name(upload_path)))
        )
        upload_s3_file(
            powerloader_binary, up_path, server=file["s3_mock_server"], plain_http=True
        )
        Path(upload_path).unlink()
        server = file["s3_mock_server"] + "/" + str(file["s3_bucketname"])
        generate_s3_download_yml(file, server, path_to_name(upload_path))
        download_s3_file(powerloader_binary, file, plain_http=True)
        assert hash_before_upload == calculate_sha256(upload_path)

    @pytest.mark.skipif(
        ifnone("AWS_SECRET_ACCESS_KEY")
        or ifnone("AWS_ACCESS_KEY_ID")
        or ifnone("AWS_DEFAULT_REGION"),
        reason="Environment variable(s) not defined",
    )
    def test_yml_s3_mock_mirror(self, file, checksums, powerloader_binary):
        remove_all(file)
        filename = str(file["s3_bucketname"]) + "/" + path_to_name(file["xtensor_path"])
        generate_s3_download_yml(file, file["s3_mock_server"], filename)
        download_s3_file(powerloader_binary, file, plain_http=True)
        Path(file["tmp_yml"]).unlink()

        for fp in get_files(file):
            assert calculate_sha256(fp) == checksums[str(path_to_name(fp))]
