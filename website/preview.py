"""Loopback-only preview of the generated showcase, never the source checkout."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', type=int, default=4388)
args = parser.parse_args()
site = ROOT / 'dist/website'
if not (site / 'showcase-files.json').is_file(): raise SystemExit('Run python website/build.py first.')
class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache')
        super().end_headers()
print(f'Local: http://127.0.0.1:{args.port}/', flush=True)
try:
    ThreadingHTTPServer(('127.0.0.1', args.port), partial(Handler, directory=str(site))).serve_forever()
except KeyboardInterrupt:
    pass
