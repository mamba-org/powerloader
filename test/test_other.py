from fixtures import *
from growing_file import *


class TestAll:
    @classmethod
    def setup_class(cls):
        pass

    @classmethod
    def teardown_class(cls):
        pass

    # Download the expected file
    def test_working_download(
        self, file, powerloader_binary, mock_server_working, checksums
    ):
        remove_all(file)

        out = subprocess.check_output(
            [
                powerloader_binary,
                "download",
                f"{mock_server_working}/static/packages/{file['name']}",
                "-o",
                file["output_path"],
            ]
        )

        assert calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert Path(file["output_path"]).exists()
        assert not Path(file["output_path_pdpart"]).exists()
        assert os.path.getsize(file["output_path"]) == file["size"]

    # Download the expected file
    def test_working_download_pwd(
        self, file, powerloader_binary, mock_server_password, checksums
    ):
        remove_all(file)

        out = subprocess.check_output(
            [
                powerloader_binary,
                "download",
                f"{mock_server_password}/static/packages/{file['name']}",
                "-o",
                file["output_path"],
            ]
        )

        assert calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert Path(file["output_path"]).exists()
        assert not Path(file["output_path_pdpart"]).exists()
        assert os.path.getsize(file["output_path"]) == file["size"]

    # Download from a path that works on the third try
    def test_broken_for_three_tries(
        self, file, powerloader_binary, mock_server_working, checksums
    ):
        remove_all(file)
        out = subprocess.check_output(
            [
                powerloader_binary,
                "download",
                f"{mock_server_working}/broken_counts/static/packages/{file['name']}",
                "-o",
                file["output_path"],
            ]
        )
        assert calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert os.path.getsize(file["output_path"]) == file["size"]

    def test_working_download_broken_checksum(
        self, file, powerloader_binary, mock_server_working
    ):
        remove_all(file)
        try:
            out = subprocess.check_output(
                [
                    powerloader_binary,
                    "download",
                    f"{mock_server_working}/static/packages/{file['name']}",
                    "--sha",
                    "broken_checksum",
                    "-o",
                    file["output_path"],
                ]
            )
        except subprocess.CalledProcessError as e:
            print(e)
        assert not Path(file["output_path_pdpart"]).exists()
        assert not Path(file["output_path"]).exists()

    # Download a broken file
    def test_broken_download_good_checksum(
        self, file, powerloader_binary, mock_server_working
    ):
        remove_all(file)
        try:
            out = subprocess.check_output(
                [
                    powerloader_binary,
                    "download",
                    f"{mock_server_working}/harm_checksum/static/packages/{file['name']}",
                    "--sha",
                    "broken_checksum",
                    "-o",
                    file["output_path"],
                ]
            )
        except subprocess.CalledProcessError as e:
            print(e)

        assert not Path(file["output_path_pdpart"]).exists()
        assert not Path(file["output_path"]).exists()

    def test_part_resume(
        self, file, powerloader_binary, mock_server_working, checksums
    ):
        # Download the expected file
        out = subprocess.check_output(
            [
                powerloader_binary,
                "download",
                f"{mock_server_working}/static/packages/{file['name']}",
                "-o",
                file["output_path"],
            ]
        )

        with open(file["output_path"], "rb") as fi:
            data = fi.read()
        with open(file["output_path_pdpart"], "wb") as fo:
            fo.write(data[0:400])

        # Resume the download
        out = subprocess.check_output(
            [
                powerloader_binary,
                "download",
                "-r",
                f"{mock_server_working}/static/packages/{file['name']}",
                "-o",
                file["output_path"],
            ]
        )
        assert calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert os.path.getsize(file["output_path"]) == file["size"]

        sent_headers = get_prev_headers(mock_server_working)
        assert "Range" in sent_headers
        assert sent_headers["Range"] == "bytes=400-"

    def test_yml_download_working(
        self,
        file,
        mirrors_with_names,
        checksums,
        powerloader_binary,
        mock_server_working,
        mock_server_404,
        mock_server_lazy,
        mock_server_broken,
        mock_server_password,
    ):
        remove_all(file)

        out = subprocess.check_output(
            [
                powerloader_binary,
                "download",
                "-f",
                file["mirrors"],
                "-d",
                file["tmp_path"],
            ]
        )

        for fn in mirrors_with_names["names"]:
            assert calculate_sha256(file["tmp_path"] / fn) == checksums[str(fn)]

    def test_yml_content_based_behavior(
        self,
        file,
        sparse_mirrors_with_names,
        checksums,
        powerloader_binary,
        mock_server_working,
        mock_server_404,
        mock_server_lazy,
        mock_server_broken,
        mock_server_password,
    ):
        remove_all(file)

        out = subprocess.check_output(
            [
                powerloader_binary,
                "download",
                "-f",
                file["local_mirrors"],
                "-d",
                file["tmp_path"],
            ]
        )

        for fn in sparse_mirrors_with_names["names"]:
            assert calculate_sha256(file["tmp_path"] / fn) == checksums[str(fn)]

    def test_yml_password_format_one(
        self,
        file,
        sparse_mirrors_with_names,
        checksums,
        powerloader_binary,
        mock_server_working,
        mock_server_404,
        mock_server_lazy,
        mock_server_broken,
        mock_server_password,
    ):
        remove_all(file)

        out = subprocess.check_output(
            [
                powerloader_binary,
                "download",
                "-f",
                file["authentication"],
                "-d",
                file["tmp_path"],
            ]
        )

        for fn in sparse_mirrors_with_names["names"]:
            assert calculate_sha256(file["tmp_path"] / fn) == checksums[str(fn)]

    # TODO: Parse outputs?, Randomized tests?
    def test_yml_with_interruptions(
        self,
        file,
        sparse_mirrors_with_names,
        checksums,
        powerloader_binary,
        mock_server_working,
        mock_server_404,
        mock_server_lazy,
        mock_server_broken,
        mock_server_password,
    ):
        remove_all(file)
        out = subprocess.check_output(
            [
                powerloader_binary,
                "download",
                "-f",
                file["local_mirrors"],
                "-d",
                file["tmp_path"],
                "-v",
            ]
        )

        pdp = ".pdpart"
        for fn in sparse_mirrors_with_names["names"]:
            fp = file["tmp_path"] / fn
            with open(fp, "rb") as fi:
                data = fi.read()

            with open(str(fp) + pdp, "wb") as fo:
                fo.write(data[0:400])
            fp.unlink()

        # The servers is reliable now
        for broken_file in filter_broken(get_files(file), pdp):
            fn = path_to_name(broken_file.replace(pdp, ""))
            fp = Path(file["tmp_path"]) / Path(fn)
            out = subprocess.check_output(
                [
                    powerloader_binary,
                    "download",
                    "-r",
                    f"{mock_server_working}/static/packages/{fn}",
                    "-o",
                    fp,
                ]
            )

        for fn in sparse_mirrors_with_names["names"]:
            assert calculate_sha256(file["tmp_path"] / fn) == checksums[str(fn)]

    def test_zchunk_basic(file, powerloader_binary, mock_server_working):
        # Download the expected file
        assert not Path("lorem.txt.zck").exists()

        out = subprocess.check_output(
            [
                powerloader_binary,
                "download",
                f"{mock_server_working}/static/zchunk/lorem.txt.zck",
                "--zck-header-size",
                "257",
                "--zck-header-sha",
                "57937bf55851d111a497c1fe2ad706a4df70e02c9b8ba3698b9ab5f8887d8a8b",
            ]
        )

        assert Path("lorem.txt.zck").exists()
        Path("lorem.txt.zck").unlink()

    def test_zchunk_basic_extract(file, powerloader_binary, mock_server_working):
        # Download the expected file
        assert not Path("lorem.txt.zck").exists()
        assert not Path("lorem.txt").exists()

        # TODO: zck_read_header ./test/conda_mock/static/zchunk/lorem.txt.zck
        # Use `Header size` and `Header checksum` rather than hard coding it into the script...
        out = subprocess.check_output(
            [
                powerloader_binary,
                "download",
                f"{mock_server_working}/static/zchunk/lorem.txt.zck",
                "-x",
                "--zck-header-size",
                "257",
                "--zck-header-sha",
                "57937bf55851d111a497c1fe2ad706a4df70e02c9b8ba3698b9ab5f8887d8a8b",
            ]
        )
        raise Exception("Stop here!")

        assert Path("lorem.txt.zck").exists()
        assert Path("lorem.txt").exists()
        Path("lorem.txt.zck").unlink()
        Path("lorem.txt").unlink()

    def test_zchunk_random_file(self, file):
        remove_all(file)
        name = "_random_file"

        path1 = str(
            Path(file["tmp_path"]) / Path(str(platform.system()) + name + "1.txt")
        )
        path2 = str(
            Path(file["tmp_path"]) / Path(str(platform.system()) + name + "2.txt")
        )
        path3 = str(
            Path(file["tmp_path"]) / Path(str(platform.system()) + name + "3.txt")
        )
        exponent = 20
        generate_random_file(path1, size=2 ** exponent)
        generate_random_file(path2, size=2 ** exponent)

        # opening first file in append mode and second file in read mode
        f1 = open(path1, "rb")
        f2 = open(path2, "rb")
        f3 = open(path3, "a+b")

        # appending the contents of the second file to the first file
        f3.write(f1.read())
        f3.write(f2.read())

        # Comput zchunks
        out1 = subprocess.check_output(["zck", path1, "-o", path1 + ".zck"])
        out2 = subprocess.check_output(["zck", path2, "-o", path2 + ".zck"])
        out3 = subprocess.check_output(["zck", path3, "-o", path3 + ".zck"])

        # Check delta size
        dsize1 = subprocess.check_output(
            ["zck_delta_size", path1 + ".zck", path3 + ".zck"]
        )
        dsize2 = subprocess.check_output(
            ["zck_delta_size", path2 + ".zck", path3 + ".zck"]
        )

        pf_1, pch_1, num_chunks_1 = get_percentage(dsize1)
        pf_2, pch_2, num_chunks_2 = get_percentage(dsize2)

        print(
            "Will download "
            + str(round(pf_1))
            + "% of file1, that's "
            + str(round(pch_1))
            + "% of chunks. Total: "
            + str(num_chunks_1)
            + " chunks."
        )
        print(
            "Will download "
            + str(round(pf_2))
            + "% of file2, that's "
            + str(round(pch_2))
            + "% of chunks. Total: "
            + str(num_chunks_2)
            + " chunks."
        )

        assert round(pf_1) < 65
        assert round(pf_2) < 65

    def test_growing_file(self, file, powerloader_binary, mock_server_working):
        remove_all(file)

        name = Path("static/zchunk/growing_file/gf" + str(platform.system()))
        filepath = file["test_path"] / Path("conda_mock") / name

        headers = get_header_map(str(filepath) + ".zck")

        print("Headers: " + str(headers))

        command = [
            powerloader_binary,
            "download",
            f"{mock_server_working}/" + str(name) + ".zck",
            "--zck-header-size",
            headers["Header size"],
            "--zck-header-sha",
            headers["Header checksum"],
        ]

        # print("Command: " + str(command))
        # time.sleep(10000)
        # raise Exception("Stop here!")
        out = subprocess.check_output(command)

        """
        for i in range(100):
            success, percentage = gf.add_content()
            print(percentage)
            if success == False:
                raise Exception("Stop here!")
        """
