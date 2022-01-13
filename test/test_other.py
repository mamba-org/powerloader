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

    def test_zchunk_basic(self, file, powerloader_binary, mock_server_working):
        # Download the expected file
        name = Path("lorem.txt.zck")
        localpath = file["tmp_path"] / name

        assert not localpath.exists()
        filepath = file["lorem_zck"] / name
        get_zchunk(
            file,
            filepath,
            "static/zchunk/" + str(name),
            powerloader_binary,
            mock_server_working,
            outpath=localpath,
        )
        assert localpath.exists()
        localpath.unlink()

    def test_zchunk_exact(self, file, powerloader_binary, mock_server_working):
        # Download the expected file
        name = Path("lorem.txt.zck")
        localpath = file["tmp_path"] / name
        filepath = file["lorem_zck"] / name
        get_zchunk(
            file,
            filepath,
            "static/zchunk/" + str(name),
            powerloader_binary,
            mock_server_working,
            outpath=localpath,
            extra_params=["-x"],
        )
        assert localpath.exists()
        localpath.unlink()

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

        map1 = get_percentage(dsize1, get_header_map(path1 + ".zck"))
        map2 = get_percentage(dsize2, get_header_map(path2 + ".zck"))

        print(
            "Will download "
            + str(round(map1["percentage to download"]))
            + "% of file1, that's "
            + str(round(map2["percentage matched chunks"]))
            + "% of chunks."
        )
        print(
            "Will download "
            + str(round(map2["percentage to download"]))
            + "% of file2, that's "
            + str(round(map2["percentage matched chunks"]))
            + "% of chunks."
        )

        assert map1["percentage to download"] < 65
        assert map2["percentage to download"] < 65

    def test_growing_file(self, file, powerloader_binary, mock_server_working):

        remove_all(file)

        name = Path("static/zchunk/growing_file/gf" + str(platform.system()) + ".zck")
        filepath = file["test_path"] / Path("conda_mock") / name
        outpath = file["tmp_path"] / name.name

        for i in range(16):
            get_zchunk(
                file, filepath, name, powerloader_binary, mock_server_working, outpath
            )
            resize_zchunk(powerloader_binary, mock_server_working)
            percentage_map = get_zck_percent_delta(outpath)
            if percentage_map != False:
                print("percentage_map: " + str(percentage_map))
                # print("sha256: " + str(calculate_sha256(outpath)))
                if percentage_map["header size / data size"] >= (1 * 10 ** -2):
                    assert percentage_map["percentage to download"] > 50
                elif percentage_map["header size / data size"] <= (6 * 10 ** -4):
                    assert percentage_map["percentage to download"] < 50
