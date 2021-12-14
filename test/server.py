from http.server import HTTPServer
from argparse import ArgumentParser
# from optparse import OptionParser

try:
    from conda_mock.conda_mock import conda_mock_handler
except (ValueError, ImportError):
    from .conda_mock.conda_mock import conda_mock_handler

def start_server(port, broken, err_type, host="127.0.0.1", handler=None):
    if handler is None:
        handler = conda_mock_handler(port, broken, err_type)
    print(f"Starting server with {port} on {host}")
    print(f"Missing packages: {broken}\n")
    print("Server started!")
    with HTTPServer((host, port), handler) as server:
        server.serve_forever()
    print("ended")

if __name__ == '__main__':
    parser = ArgumentParser()

    parser.add_argument("-p", "--port", default=5555, type=int)
    parser.add_argument("-n", "--host", default="127.0.0.1")
    parser.add_argument("-e", "--error_type", default="404")
    parser.add_argument('--pkgs', metavar='N', type=str, nargs='+', help='broken pkgs')

    args = parser.parse_args()
    start_server(args.port, set(args.pkgs), args.error_type, args.host, handler=None)
