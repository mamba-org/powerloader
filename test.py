import sys

sys.path.append("./build/src/python/")

import pypowerloader

pypowerloader.hello_world()

path = "linux-64/python-3.9.7-hb7a2778_1_cpython.tar.bz2"
baseurl = "https://conda.anaconda.org/conda-forge"
filename = "python3.9_test"
downTarg = pypowerloader.DownloadTarget(path, baseurl, filename)
print("complete url: " + downTarg.complete_url)


def progress(total, done):
    print(f"Total {total}, done {done}")
    return 0


downTarg.progress_callback = progress

dl = pypowerloader.Downloader()
dl.add(downTarg)

con = pypowerloader.Context()

# dl.download()
mirror = pypowerloader.Mirror(baseurl)

print("mirror_map1: " + str(con.mirror_map))
con.mirror_map = {"conda-forge": [mirror], "test": []}
print("mirror_map2: " + str(con.mirror_map))
