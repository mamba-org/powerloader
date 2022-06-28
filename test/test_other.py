from distutils import command
from shutil import copyfile
from growing_file import *
from fixtures import *


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

        command = [
            powerloader_binary,
            "download",
            f"{mock_server_working}/static/packages/{file['name']}",
            "-o",
            file["output_path"],
        ]

        out = run_command(command)

        assert calculate_sha256(file["output_path"]) == checksums[file["name"]]
        assert Path(file["output_path"]).exists()
        assert not Path(file["output_path_pdpart"]).exists()
        assert os.path.getsize(file["output_path"]) == file["size"]

    # Download the expected file
    def test_working_download_pwd(
        self, file, powerloader_binary, mock_server_password, checksums
    ):
        remove_all(file)

        command = [
            powerloader_binary,
            "download",
            f"{mock_server_password}/static/packages/{file['name']}",
            "-o",
            file["output_path"],
        ]

        out = run_command(command)

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

    def test_broken_file(self, file, powerloader_binary, mock_server_working):
        p = Path("randomnonexist.txt")
        ppart = Path("randomnonexist.txt.pdpart")
        assert not p.exists()
        assert not ppart.exists()

        with pytest.raises(subprocess.CalledProcessError):
            out = subprocess.check_output(
                [
                    powerloader_binary,
                    "download",
                    f"{mock_server_working}/randomnonexist.txt",
                ]
            )

        assert not p.exists()
        assert not ppart.exists()

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
        filepath = file["lorem_zck"] / name
        assert not localpath.exists()

        get_zchunk_regular(
            file,
            filepath,
            "static/zchunk/" + str(name),
            powerloader_binary,
            mock_server_working,
            outpath=localpath,
        )

        headers = get_prev_headers(mock_server_working, 2)
        assert headers[0]["Range"] == "bytes=0-256"
        assert headers[1]["Range"] == "bytes=257-4822"

        assert localpath.exists()
        assert localpath.stat().st_size == 4823
        assert not Path(str(localpath) + ".pdpart").exists()

        # Confirm that no further content is downloaded
        clear_prev_headers(mock_server_working)
        assert localpath.exists()
        assert not Path(str(localpath) + ".pdpart").exists()

        get_zchunk_regular(
            file,
            filepath,
            "static/zchunk/" + str(name),
            powerloader_binary,
            mock_server_working,
            outpath=localpath,
        )

        assert localpath.exists()
        assert not Path(str(localpath) + ".pdpart").exists()
        headers = get_prev_headers(mock_server_working, 100)
        assert headers is None

        assert localpath.exists()
        assert localpath.stat().st_size == 4823
        assert not Path(str(localpath) + ".pdpart").exists()

        # grow the file by tripling the original
        root = proj_root()

        new_name = Path("lorem.txt.x3.zck")
        new_filepath = file["lorem_zck"] / new_name

        clear_prev_headers(mock_server_working)

        get_zchunk_regular(
            file,
            filepath=new_filepath,
            name="static/zchunk/" + str(new_name),
            powerloader_binary=powerloader_binary,
            mock_server_working=mock_server_working,
            outpath=localpath,
        )

        headers = get_prev_headers(mock_server_working, 100)
        assert len(headers) == 2

        # lead
        assert headers[0]["Range"] == "bytes=0-257"
        # header
        range_start = int(headers[1]["Range"][len("bytes=") :].split("-")[0])
        assert range_start > 4000

        assert localpath.stat().st_size == new_filepath.stat().st_size
        assert calculate_sha256(new_filepath) == calculate_sha256(str(localpath))
        assert localpath.exists()
        assert not Path(str(localpath) + ".pdpart").exists()
        localpath.unlink()

    def test_zchunk_extract(self, file, powerloader_binary, mock_server_working):
        # Download the expected file
        name = Path("lorem.txt.zck")
        name_extracted = Path("lorem.txt")
        localpath = file["tmp_path"] / name
        filepath = file["lorem_zck"] / name
        extracted_fp = localpath.parents[0] / name_extracted
        get_zchunk_regular(
            file,
            filepath,
            "static/zchunk/" + str(name),
            powerloader_binary,
            mock_server_working,
            outpath=localpath,
            extra_params=["-x"],
        )

        assert localpath.exists()
        assert extracted_fp.exists()
        assert not Path(str(localpath) + ".pdpart").exists()
        extracted_fp.unlink()
        localpath.unlink()

    def test_zchunk_basic_nochksum(
        self, file, powerloader_binary, mock_server_working, unique_filename
    ):
        # Download the expected file
        name = Path("lorem.txt.zck")
        localpath = file["tmp_path"] / name
        filepath = file["lorem_zck"] / name
        assert not localpath.exists()

        get_zchunk_regular(
            file,
            filepath,
            "static/zchunk/" + str(name),
            powerloader_binary,
            mock_server_working,
            outpath=localpath,
        )

        headers = get_prev_headers(mock_server_working, 2)
        assert headers[0]["Range"] == "bytes=0-256"
        assert headers[1]["Range"] == "bytes=257-4822"
        assert localpath.exists()
        localpath.unlink()

    def test_zchunk_random_file(self, file, unique_filename):
        remove_all(file)
        name = "_random_file"

        path1 = str(
            Path(file["tmp_path"]) / Path(str(unique_filename) + name + "1.txt")
        )
        path2 = str(
            Path(file["tmp_path"]) / Path(str(unique_filename) + name + "2.txt")
        )
        path3 = str(
            Path(file["tmp_path"]) / Path(str(unique_filename) + name + "3.txt")
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

        # Compute zchunks
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

    def test_growing_file(
        self,
        file,
        powerloader_binary,
        mock_server_working,
        zchunk_expectations,
        unique_filename,
    ):

        remove_all(file)
        name = Path("static/zchunk/growing_file/gf" + unique_filename + ".zck")
        filepath = file["test_path"] / Path("conda_mock") / name
        outpath = file["tmp_path"] / name.name

        for i in range(16):
            get_zchunk_regular(
                file, filepath, name, powerloader_binary, mock_server_working, outpath
            )
            percentage_map = get_zck_percent_delta(outpath, first_time=(i == 0))
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
            resize_zchunk(powerloader_binary, mock_server_working)
        # TODO: check headers
