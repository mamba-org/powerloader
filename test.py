import sys

sys.path.append("./build/src/python/")

import pypowerloader

pypowerloader.hello_world()

path = "linux-64/python-3.9.7-hb7a2778_1_cpython.tar.bz2"
baseurl = "https://conda.anaconda.org/conda-forge"
filename = "python3.9_test"
downTarg = pypowerloader.DownloadTarget(path, baseurl, filename)


def progress(total, done):
    print(f"Total {total}, done {done}")
    return 0


downTarg.progress_callback = progress

con = pypowerloader.Context()

dl = pypowerloader.Downloader(con)
dl.add(downTarg)

# dl.download()
mirror = pypowerloader.Mirror(con, baseurl)

print("mirror_map1: " + str(con.mirror_map.as_dict()))
con.mirror_map.reset({"conda-forge": [mirror], "test": []})
print("mirror_map2: " + str(con.mirror_map.as_dict()))
