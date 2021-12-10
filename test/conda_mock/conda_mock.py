import base64
from http.server import BaseHTTPRequestHandler, HTTPServer
import os
import sys
import time

from .config import AUTH_USER, AUTH_PASS

def file_path(path):
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), path)

failure_count = 0

def conda_mock_handler(port):

    class CondaMockHandler(BaseHTTPRequestHandler):
        _port = port
        count_thresh, time_thresh = 3, 7
        start_time = time.time()

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
                self.serve_static()

        def return_not_found_temporary(self):
            global failure_count
            reference = 1.1 * self.time_thresh + time.time() - self.start_time
            if ((reference % 10) < self.time_thresh) and failure_count < self.count_thresh:
                return self.serve_static()
            else:
                failure_count += 1
                return self.return_not_found()

        def reset_failure_count(self):
            global failure_count
            failure_count = 0
            self.send_response(200)
            self.end_headers()

        def return_ok_with_message(self, message, content_type='text/html'):
            if content_type == 'text/html':
                message = bytes(message, 'utf8')
            self.send_response(200)
            self.send_header('Content-type', content_type)
            self.send_header('Content-Length', str(len(message)))
            self.end_headers()
            self.wfile.write(message)

        def parse_path(self, test_prefix='', keyword_expected=False):
            keyword, path = "", self.path[len(test_prefix):]
            if keyword_expected:
                keyword, path = path.split('/', 1)
            # Strip arguments
            if '?' in path:
                path = path[:path.find('?')]
            if keyword_expected:
                return keyword, path
            return path

        def serve_harm_checksum(self):
            """Append two newlines to content of a file (from the static dir) with
            specified keyword in the filename. If the filename doesn't contain
            the keyword, content of the file is returnen unchanged."""
            keyword, path = self.parse_path('', keyword_expected=True)
            self.serve_file(path, harm_keyword=keyword)

        def serve_file(self, path, harm_keyword=None):
            if "static/" not in path:
                # Support changing only files from static directory
                return self.return_bad_request()

            path = path[path.find("static/"):]
            try:
                with open(file_path(path), 'rb') as f:
                    data = f.read()
                    if harm_keyword is not None and harm_keyword in os.path.basename(file_path(path)):
                        data += b"\n\n"
                    return self.return_ok_with_message(data, 'application/octet-stream')
            except IOError:
                # File probably doesn't exist or we can't read it
                return self.return_not_found()

        def serve_static(self):
            path = self.parse_path()
            self.serve_file(path)

        def do_GET(self):
            """
            Add specific hooks if needed
            :return:
            """

            if self.path.startswith('/broken_counts/static/'):
                return self.return_not_found_counts()

            if self.path.startswith('/broken_temporary/static/'):
                return self.return_not_found_temporary()

            if self.path.startswith('/reset_broken_count'):
                return self.reset_failure_count()

            if self.path.startswith('/harm_checksum/static/'):
                return self.serve_harm_checksum()

            return self.serve_static()

    return CondaMockHandler
