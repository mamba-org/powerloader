import sys

sys.path.append("./build/src/python/")

import pypowerloader

pypowerloader.hello_world()

path = "linux-64/python-3.9.7-hb7a2778_1_cpython.tar.bz2"
baseurl = "https://conda.anaconda.org/conda-forge"
filename = "python3.9_test"
downTarg = pypowerloader.DownloadTarget(path, baseurl, filename)
print("complete url: " + downTarg.complete_url)

mirror = pypowerloader.Mirror(baseurl)
dl = pypowerloader.Downloader()
dl.add(downTarg)
dl.download()
