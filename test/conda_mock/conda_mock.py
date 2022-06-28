from http.server import BaseHTTPRequestHandler, SimpleHTTPRequestHandler, HTTPServer
import os, sys, time, re, json
import hashlib, base64
from helpers import *
from .config import AUTH_USER, AUTH_PASS


def file_path(path):
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), path)


failure_count = 0
prev_headers = []
BYTE_RANGE_RE = re.compile(r"bytes=(\d+)-(\d+)?$")


def parse_byte_range(byte_range):
    """Returns the two numbers in 'bytes=123-456' or throws ValueError.

    The last number or both numbers may be None.
    """
    if byte_range.strip() == "":
        return None, None

    m = BYTE_RANGE_RE.match(byte_range)
    if not m:
        raise ValueError("Invalid byte range %s" % byte_range)

    first, last = [x and int(x) for x in m.groups()]
    if last and last < first:
        raise ValueError("Invalid byte range %s" % byte_range)
    return first, last


def conda_mock_handler(
    port, pkgs, err_type, username, pwd, host, content_path, output_file
):
    class CondaMockHandler(BaseHTTPRequestHandler):
        content_present, gf = setup_file(content_path, output_file)
        _port, _pkgs, _err_type = port, pkgs, err_type
        _username, _pwd = username, pwd
        count_thresh = 3
        number_of_servers = 4

        def return_bad_request(self):
            self.send_response(400)
            self.end_headers()

        def return_not_found(self):
            self.send_response(404)
            self.end_headers()

        def return_not_found_counts(self):
            global failure_count
            failure_count += 1
            if failure_count < self.count_thresh:
                self.return_not_found()
            else:
                failure_count = 0
                self.serve_static()

        def reset_failure_count(self):
            global failure_count
            failure_count = 0
            self.send_response(200)
            self.end_headers()

        def return_ok_with_message(self, message, content_type="text/html"):
            if content_type == "text/html":
                message = bytes(message, "utf8")
            self.send_response(200)
            self.send_header("Content-type", content_type)
            self.send_header("Content-Length", str(len(message)))
            self.end_headers()
            self.wfile.write(message)

        def parse_path(self, test_prefix="", keyword_expected=False):
            keyword, path = "", self.path[len(test_prefix) :]
            if keyword_expected:
                keyword, path = path.split("/", 1)
            # Strip arguments
            if "?" in path:
                path = path[: path.find("?")]
            if keyword_expected:
                return keyword, path
            return path

        def grow_file(self):
            self.gf.add_content()
            self.send_response(200)
            self.end_headers()

        def serve_growing_file(self):
            path = self.parse_path()
            return self.serve_file(path)

        def serve_harm_checksum(self):
            """Append two newlines to content of a file (from the static dir) with
            specified keyword in the filename. If the filename doesn't contain
            the keyword, content of the file is returnen unchanged."""
            keyword, path = self.parse_path("", keyword_expected=True)
            return self.serve_file(path, harm_keyword=keyword)

        def serve_range_data(self, data, content_type):
            first, last = self.range
            print(f"serving {first} -> {last}")
            if first >= len(data):
                self.send_error(416, "Requested Range Not Satisfiable")
                return None

            self.send_response(206)
            self.send_header("Accept-Ranges", "bytes")
            if last is None or last >= len(data):
                last = len(data) - 1
            response_length = last - first + 1
            self.send_header("Content-type", content_type)
            self.send_header(
                "Content-Range", "bytes %s-%s/%s" % (first, last, len(data))
            )
            self.send_header("Content-Length", str(response_length))
            # self.send_header('Last-Modified', self.date_time_string(fs.st_mtime))
            self.end_headers()
            self.wfile.write(data[first : last + 1])

        def clear_prev_headers(self):
            global prev_headers
            prev_headers = []
            return self.return_ok_with_message("OK")

        def serve_prev_headers(self):
            if not prev_headers:
                self.return_ok_with_message(
                    json.dumps(None).encode("utf-8"), "application/json"
                )

            res = []
            for el in prev_headers:
                d = {}
                for k in el.keys():
                    d[k] = el[k]
                res.append(d)
            self.return_ok_with_message(
                json.dumps(res).encode("utf-8"), "application/json"
            )

        def serve_file(self, path, harm_keyword=None):
            global prev_headers
            prev_headers.append(self.headers)

            if "Range" not in self.headers:
                self.range = None
            else:
                try:
                    self.range = parse_byte_range(self.headers["Range"])
                except ValueError as e:
                    self.send_error(400, "Invalid byte range")
                    return None
                first, last = self.range

            if "static/" not in path:
                # Support changing only files from static directory
                return self.return_bad_request()

            path = path[path.find("static/") :]
            try:
                with open(file_path(path), "rb") as f:
                    data = f.read()
                    if harm_keyword is not None and harm_keyword in os.path.basename(
                        file_path(path)
                    ):
                        data += b"\n\n"
                    if self.range:
                        return self.serve_range_data(data, "application/octet-stream")
                    return self.return_ok_with_message(data, "application/octet-stream")
            except IOError:
                # File probably doesn't exist or we can't read it
                return self.return_not_found()

        def select_error(self, err_type):
            # possible errors = 404, boken, lazy
            if err_type == "404":
                return self.return_not_found()
            elif err_type == "broken":
                return self.serve_harm_checksum()
            elif err_type == "lazy":
                return self.return_not_found_counts()
            path = self.parse_path()
            return self.serve_file(path)

        def get_filename(self):
            filename = self.path.split("/")[-1]
            return filename

        def serve_static(self):
            if self.get_filename() in pkgs:
                return self.select_error(err_type)
            else:
                path = self.parse_path()
                return self.serve_file(path)

        def do_HEAD(self):
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()

        def do_AUTHHEAD(self):
            self.send_response(401)
            self.send_header("WWW-Authenticate", 'Basic realm="Test"')
            self.send_header("Content-type", "text/html")
            self.end_headers()

        def get_main(self):
            if self.path.startswith("/prev_headers"):
                return self.serve_prev_headers()
            if self.path.startswith("/clear_prev_headers"):
                return self.clear_prev_headers()

            if self.path.startswith("/broken_counts/static/"):
                return self.return_not_found_counts()

            if self.path.startswith("/reset_broken_count"):
                return self.reset_failure_count()

            if self.path.startswith("/harm_checksum/static/"):
                return self.serve_harm_checksum()

            if self.path.startswith("/static/zchunk/growing_file"):
                if not self.content_present:
                    raise Exception("Didn't specify a path to the content!")
                return self.serve_growing_file()

            if self.path.startswith("/add_content"):
                if not self.content_present:
                    raise Exception("Didn't specify a path to the content!")
                return self.grow_file()

            return self.serve_static()

        def do_GET(self):
            """
            Add specific hooks if needed
            :return:
            """
            # "user:passwort" # os.environ["TESTPWD"]
            key = username + ":" + pwd

            if key == ":":
                # Workaround, because we don't support empty usernames and passwords
                self.get_main()
            else:
                key = base64.b64encode(bytes(key, "utf-8")).decode("ascii")
                """ Present frontpage with user authentication. """
                auth_header = self.headers.get("Authorization", "")

                if not auth_header:
                    self.do_AUTHHEAD()
                    self.wfile.write(b"no auth header received")
                    return True
                elif auth_header == "Basic " + key:
                    # SimpleHTTPRequestHandler.do_GET(self)
                    return self.get_main()
                else:
                    self.do_AUTHHEAD()
                    self.wfile.write(auth_header.encode("ascii"))
                    self.wfile.write(b"not authenticated")
                    return True

    return CondaMockHandler
