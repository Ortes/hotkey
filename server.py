import os
import ssl
from http.server import HTTPServer, BaseHTTPRequestHandler


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-type', 'application/octet-stream')
        self.send_header('Content-Length', str(os.stat("build/hotkey.bin").st_size))
        self.end_headers()
        self.wfile.write(open("build/hotkey.bin", "rb").read())


httpd = HTTPServer(('0.0.0.0', 34271), Handler)
httpd.socket = ssl.wrap_socket(httpd.socket, certfile='../firmware.crt', keyfile='../firmware.key', server_side=True)
httpd.serve_forever()
