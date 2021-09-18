![](docs/assets/logo.png)

# The POWERLOADER

**Note: the code in here is very, very much a work in progress. Please do not use it!**

This is a tool to download large files. This is to be used in `mamba` and potentially other package managers. It's a port of `librepo`, but extends it in several ways as well as being cross-platform (it should support Windows as well).

Current WIP features are:

- Mirror support and automatic mirror selection
- Native OCI registry and S3 bucket support
- Resumable downloads
- zchunk support for delta-downloads

In the future this might be directly integrated into the `mamba` codebase -- or live seperately.
