from os import unlink
from fixtures import *


@pytest.fixture
def zck_file_upload_aws_server(powerloader_binary, xprocess, fs_ready, file):
    upload_path = file["lorem_zck_file"]
    up_path = str(upload_path) + ":" + path_to_name(upload_path)
    destination1 = file["tmp_path"] / upload_path.name
    assert not destination1.exists()

    upload_path = file["lorem_zck_file_x3"]
    up_path = str(upload_path) + ":" + path_to_name(upload_path)
    destination3x = file["tmp_path"] / upload_path.name
    assert not destination3x.exists()

    srv_name = upload_s3_file(
        powerloader_binary, up_path, server=file["s3_server"], plain_http=False
    )

    srv_name_x3 = upload_s3_file(
        powerloader_binary, up_path, server=file["s3_server"], plain_http=False
    )

    get_zchunk_s3(
        file,
        filepath=file["lorem_zck_file"],
        powerloader_binary=powerloader_binary,
        localpath=destination1,
        mock=False,
    )

    get_zchunk_s3(
        file,
        filepath=file["lorem_zck_file_x3"],
        powerloader_binary=powerloader_binary,
        localpath=file["tmp_path"] / upload_path.name,
        mock=False,
    )

    assert destination1.exists()
    assert destination3x.exists()
    assert calculate_sha256(destination1) == calculate_sha256(file["lorem_zck_file"])
    assert calculate_sha256(destination3x) == calculate_sha256(
        file["lorem_zck_file_x3"]
    )
    destination1.unlink()
    destination3x.unlink()

    yield srv_name, srv_name_x3
    # TODO: Remove files from remote again


class TestS3Server:
    @skip_aws_credentials
    def test_up_and_down(self, file, powerloader_binary, unique_filename):
        remove_all(file)
        upload_path = generate_unique_file(file, unique_filename)
        hash_before_upload = calculate_sha256(upload_path)
        up_path = upload_path + ":" + path_to_name(upload_path)
        upload_s3_file(
            powerloader_binary, up_path, server=file["s3_server"], plain_http=False
        )
        Path(upload_path).unlink()
        generate_s3_download_yml(file, file["s3_server"], path_to_name(upload_path))
        download_s3_file(powerloader_binary, file)
        assert hash_before_upload == calculate_sha256(upload_path)

    @skip_aws_credentials
    def test_zchunk_basic(self, file, powerloader_binary, zck_file_upload_aws_server):
        # Download the expected file
        name = file["lorem_zck_file"].name
        localpath = file["tmp_path"] / name
        assert not localpath.exists()

        get_zchunk_s3(
            file,
            filepath=file["lorem_zck_file"],
            powerloader_binary=powerloader_binary,
            localpath=localpath,
            mock=False,
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
            mock=False,
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
            mock=False,
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
        up_path = str(remote_zck_path) + ":" + path_to_name(remote_zck_path)

        upload_s3_file(
            powerloader_binary, up_path, server=file["s3_server"], plain_http=False,
        )
        remote_plain_path = Path(str(remote_zck_path).replace(".zck", ""))

        # Only going to five, to save costs...
        for i in range(5):
            get_zchunk_s3(
                file,
                remote_zck_path,
                powerloader_binary,
                localpath=local_zck_path,
                mock=False,
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
            upload_s3_file(
                powerloader_binary, up_path, server=file["s3_server"], plain_http=False
            )
        # TODO: Check headers!
