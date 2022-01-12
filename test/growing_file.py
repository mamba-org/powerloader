from helpers import *


class GrowingFile:
    max_exponent = 26
    content = "".encode("utf-8")

    def get_percentage(self, delta_size, header_map):
        map = {}
        dsize_list = delta_size.decode("utf-8").split(" ")
        of_idx = [i for i, val in enumerate(dsize_list) if val == "of"]
        map["percentage to download"] = round(
            100 * float(dsize_list[of_idx[0] - 1]) / float(dsize_list[of_idx[0] + 1])
        )
        map["percentage matched chunks"] = round(
            100 * float(dsize_list[of_idx[1] - 1]) / float(dsize_list[of_idx[1] + 1])
        )
        map["header size"] = header_map["Header size"]
        map["data size"] = header_map["Data size"]
        return map

    def set_size(self, exponent):
        self.exponent = exponent
        self.size = min(2 ** exponent, 2 ** self.max_exponent)
        success = 2 ** exponent <= 2 ** self.max_exponent
        if success == False:
            self.exponent = self.initial_exponent
            self.content = "".encode("utf-8")
        return success

    def add_content(self):
        self.content += os.urandom(self.size)

        with open(self.path, "wb") as fout:
            fout.write(self.content)
        out = subprocess.check_output(
            ["zck", str(self.path), "-o", str(self.path) + ".zck"]
        )
        percentage = -1
        try:
            delta = subprocess.check_output(
                ["zck_delta_size", str(self.path) + ".zck", str(self.path) + "_old.zck"]
            )
            print("filepath: 2: " + str(self.path))
            header_map = get_header_map(str(self.path) + ".zck")
            percentage = self.get_percentage(delta, header_map)
        except Exception as e:
            print("Exception: " + str(e))
        shutil.copy(str(self.path) + ".zck", str(self.path) + "_old.zck")
        return self.set_size(exponent=self.exponent + 1), percentage

    def __init__(self, path, initial_exponent):
        self.initial_exponent = initial_exponent
        self.set_size(initial_exponent)
        self.path = path
        self.add_content()

    def __del__(self):
        pass
