![](docs/assets/logo.png)

[![Conda Version](https://img.shields.io/conda/vn/conda-forge/powerloader.svg)](https://github.com/conda-forge/powerloader-feedstock)

----

# The POWERLOADER

This is a tool to download large files. This is to be used in `mamba` and potentially other package managers. It's a port of `librepo`, but extends it in several ways as well as being cross-platform (it should support Windows as well).

Current features are:

- Mirror support and automatic mirror selection
- Native OCI registry and S3 bucket support
- Resumable downloads
- zchunk support for delta-downloads

In the future this might be directly integrated into the `mamba` codebase -- or live seperately.

### Try it out

Install dependencies (remove `zchunk` on Windows or where it's not available):

`mamba env create -n powerloader -f environment.yml`

Then you can run

```
conda activate test

mkdir build; cd build
cmake .. -GNinja
ninja

./powerloader --help
```

### Uploading files

The following uplaods the xtensor-0.24.0.tar.bz2 file to the xtensor:0.24.0 name/tag on ghcr.io.
The file will appear under the user authenticated by the GHA_TOKEN.

`powerloader upload xtensor-0.24.0.tar.bz2:xtensor:0.24.0 -m oci://ghcr.io`
