from helpers import *


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
        with open(self.plain_path, "w") as fout:
            fout.write(self.content[: self.size - 1])
        out = subprocess.check_output(
            ["zck", str(self.plain_path), "-o", str(self.path)]
        )
        return (
            self.set_size(exponent=self.exponent + 1),
            get_zck_percent_delta(self.path),
        )

    def __init__(self, path, content, initial_exponent):
        self.content, self.initial_exponent = content, initial_exponent
        self.set_size(initial_exponent)
        self.plain_path = str(path).replace(".zck", "")
        self.path = path

    def __del__(self):
        pass
