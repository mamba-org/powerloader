from http.server import HTTPServer
from optparse import OptionParser

try:
    from conda_mock.conda_mock import conda_mock_handler
except (ValueError, ImportError):
    from .conda_mock.conda_mock import conda_mock_handler

def start_server(port, host="127.0.0.1", handler=None):
    if handler is None:
        handler = conda_mock_handler(port)
    print(f"Starting server with {port} on {host}\n")
    print("Server started!")
    with HTTPServer((host, port), handler) as server:
        server.serve_forever()
    print("ended")

if __name__ == '__main__':
    parser = OptionParser("%prog [options]")
    parser.add_option(
        "-p", "--port",
        default=5555,
        type="int",
    )
    parser.add_option(
        "-n", "--host",
        default="127.0.0.1",
    )
    options, args = parser.parse_args()

    start_server(options.port, options.host)
