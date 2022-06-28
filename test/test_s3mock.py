from fixtures import *


@pytest.fixture(scope="module", autouse=True)
def mock_s3_minio(xprocess, file, fs_ready):

    set_env_var(file, "AWS_SECRET_ACCESS_KEY")
    set_env_var(file, "AWS_ACCESS_KEY_ID")
    set_env_var(file, "AWS_DEFAULT_REGION")

    assert aws_credentials_all_exist() == True

    start_docker = [
        "docker",
        "start",
        "minio",
    ]
    out = run_command(start_docker)

    if "minio" not in out:
        run_docker = [
            "docker",
            "run",
            "-d",
            "-p",
            "9000:9000",
            "--name",
            "minio",
            "-e",
            "MINIO_ACCESS_KEY=" + file["AWS_ACCESS_KEY_ID"],
            "-e",
            "MINIO_SECRET_KEY=" + file["AWS_SECRET_ACCESS_KEY"],
            "-v",
            "/tmp/data:/data",
            "-v",
            "/tmp/config:/root/.minio",
            "minio/minio",
            "server",
            "/data",
        ]
        out = run_command(run_docker)

    # Populate minIO
    # https://docs.min.io/docs/aws-cli-with-minio.html
    create_testbucket = [
        "aws",
        "--endpoint-url",
        file["s3_mock_endpoint"],
        "s3",
        "mb",
        "s3://testbucket",
    ]
    out = run_command(create_testbucket)

    aws_cp(file, file["xtensor_path"], file["s3_bucketpath"])
    aws_cp(file, file["lorem_zck_file"], file["s3_bucketpath"])
    aws_cp(file, file["lorem_zck_file_x3"], file["s3_bucketpath"])

    check_testbucket = [
        "aws",
        "--endpoint-url",
        file["s3_mock_endpoint"],
        "s3",
        "ls",
        file["s3_bucketname"],
    ]
    out = run_command(check_testbucket)
    assert "xtensor-0.23.9-hc021e02_1.tar.bz2" in out
    assert "lorem.txt.x3.zck" in out
    assert "lorem.txt.zck" in out


class TestS3Mock:
    @skip_aws_credentials
    def test_s3_mock(self, file, powerloader_binary, unique_filename):
        remove_all(file)
        upload_path = generate_unique_file(file, unique_filename)
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

    @skip_aws_credentials
    def test_s3_mock_mod_txt(self, file, powerloader_binary, unique_filename_txt):
        remove_all(file)
        upload_path = generate_unique_file(file, unique_filename_txt)
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

    @skip_aws_credentials
    def test_s3_mock_yml_mod_loc(self, file, powerloader_binary, unique_filename):
        remove_all(file)
        upload_path = generate_unique_file(file, unique_filename)
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

    @skip_aws_credentials
    def test_yml_s3_mock_mirror(self, file, checksums, powerloader_binary):
        remove_all(file)
        filename = (
            str(file["s3_bucketname"]).replace("s3://", "")
            + "/"
            + path_to_name(file["xtensor_path"])
        )
        generate_s3_download_yml(file, file["s3_mock_server"], filename)
        download_s3_file(powerloader_binary, file, plain_http=True)
        Path(file["tmp_yml"]).unlink()

        for fp in get_files(file, expect=">zero"):
            assert calculate_sha256(fp) == checksums[str(path_to_name(fp))]

    @skip_aws_credentials
    def test_zchunk_basic(self, file, powerloader_binary):
        # Download the expected file
        name = file["lorem_zck_file"].name
        localpath = file["tmp_path"] / name
        assert not localpath.exists()

        get_zchunk_s3(
            file,
            filepath=file["lorem_zck_file"],
            powerloader_binary=powerloader_binary,
            localpath=localpath,
        )
        """
        headers = get_prev_headers(mock_server_working, 2)
        assert headers[0]["Range"] == "bytes=0-256"
        assert headers[1]["Range"] == "bytes=257-4822"
        """
        assert localpath.exists()
        assert localpath.stat().st_size == 4823
        assert not Path(str(localpath) + ".pdpart").exists()

        """
        clear_prev_headers(mock_server_working)
        """
        get_zchunk_s3(
            file,
            filepath=file["lorem_zck_file"],
            powerloader_binary=powerloader_binary,
            localpath=localpath,
        )

        """
        headers = get_prev_headers(mock_server_working, 100)
        assert headers is None
        """
        assert localpath.exists()
        assert localpath.stat().st_size == 4823
        assert not Path(str(localpath) + ".pdpart").exists()

        new_name = Path("lorem.txt.x3.zck")
        new_filepath = file["lorem_zck"] / new_name

        """
        clear_prev_headers(mock_server_working)
        """

        get_zchunk_s3(
            file,
            filepath=file["lorem_zck_file_x3"],
            powerloader_binary=powerloader_binary,
            localpath=localpath,
        )

        """
        headers = get_prev_headers(mock_server_working, 100)
        assert len(headers) == 2

        # lead
        assert headers[0]["Range"] == "bytes=0-88"
        # header
        assert headers[1]["Range"] == "bytes=0-257"
        range_start = int(headers[2]["Range"][len("bytes=") :].split("-")[0])
        assert range_start > 4000
        """

        # TODO: Specify output path, so that test/tmp/lorem.txt.zck is modified
        # TODO: Download with CLI params rather than YML file, so that the headers can be checked easily
        if False:
            assert localpath.stat().st_size == new_filepath.stat().st_size
            assert calculate_sha256(new_filepath) == calculate_sha256(str(localpath))
            assert localpath.exists()
            assert not Path(str(localpath) + ".pdpart").exists()
            localpath.unlink()
        else:
            new_localpath = file["tmp_path"] / new_name
            assert new_localpath.stat().st_size == new_filepath.stat().st_size
            assert calculate_sha256(new_filepath) == calculate_sha256(
                str(new_localpath)
            )
            assert new_localpath.exists()
            assert not Path(str(new_localpath) + ".pdpart").exists()
            new_localpath.unlink()

    @skip_aws_credentials
    def test_growing_file(
        self,
        file,
        powerloader_binary,
        zchunk_expectations,
        fs_ready,
        checksums,
        unique_filename,
    ):
        name = Path("static/zchunk/growing_file/gf" + unique_filename + ".zck")
        remote_zck_path = file["test_path"] / Path("conda_mock") / name
        local_zck_path = file["tmp_path"] / name.name

        assert aws_credentials_all_exist() == True
        remove_all(file)

        content_present, gf = setup_file(
            generate_content(file, checksums), unique_filename
        )

        # Content was provided
        assert content_present == True
        aws_cp(file, remote_zck_path, file["s3_bucketpath"])
        remote_plain_path = Path(str(remote_zck_path).replace(".zck", ""))

        for i in range(16):
            get_zchunk_s3(
                file, remote_zck_path, powerloader_binary, localpath=local_zck_path
            )
            local_plain_path = Path(str(local_zck_path).replace(".zck", ""))
            assert calculate_sha256(remote_zck_path) == calculate_sha256(local_zck_path)
            assert calculate_sha256(remote_plain_path) == calculate_sha256(
                local_plain_path
            )

            percentage_map = get_zck_percent_delta(local_zck_path, first_time=(i == 0))
            if percentage_map != False:
                assert zchunk_expectations[i] == percentage_map

                if False:
                    print(
                        "\n\ni: "
                        + str(i)
                        + ", percentage_map: "
                        + str(percentage_map)
                        + "\n expectations[i]: "
                        + str(zchunk_expectations[i])
                    )

                if False:
                    print("i: " + str(i) + ", percentage_map: " + str(percentage_map))

            gf.add_content()
            aws_cp(file, remote_zck_path, file["s3_bucketpath"])
