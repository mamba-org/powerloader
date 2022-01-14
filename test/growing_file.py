from logging import raiseExceptions
from pathlib import Path
import os, subprocess, shutil


class GrowingFile:
    max_exponent = 26

    def set_size(self, exponent):
        self.exponent = exponent
        self.size = min(2 ** exponent, 2 ** self.max_exponent)
        success = 2 ** exponent <= 2 ** self.max_exponent
        if success == False:
            self.exponent = self.initial_exponent
        return success

    def add_content(self):
        with open(self.plain_path, "wb") as fout:
            content = self.content[: self.size - 1]
            fout.write(content)
        out = subprocess.check_output(
            ["zck", str(self.plain_path), "-o", str(self.path)]
        )
        new_size = self.set_size(exponent=self.exponent + 1)

    def __init__(self, path, content_path, initial_exponent):
        with open(content_path, "rb") as f:
            self.content = f.read()
        self.initial_exponent = initial_exponent
        self.set_size(initial_exponent)
        self.plain_path = str(path).replace(".zck", "")
        self.path = path
        if self.content == "":
            raise Exception("Content is required")
        self.add_content()

    def __del__(self):
        pass
