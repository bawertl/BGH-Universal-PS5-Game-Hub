"""Localhost-only DEVELOPMENT preview + library scan. Not a PS5 deployment service.

No upload, file mutation of game roots, execution or launch API exists.
The only POST handler refreshes a read-only scan and updates our own cache.
"""
from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import argparse
import json
import mimetypes
import threading
import urllib.parse

from library import load_json, scan_to_files


class LibraryServer(ThreadingHTTPServer):
    daemon_threads = True
    def __init__(self, address, handler, project: Path):
        super().__init__(address, handler)
        self.project = project
        self.ui = project / 'ui'
        self.config = project / 'config' / 'paths.json'
        self.cache = project / 'data' / 'library-cache.json'
        self.lock = threading.Lock()
        self.library = load_json(self.cache, {'schema': 2, 'games': [], 'unavailable_paths': [], 'errors': []})


class Handler(BaseHTTPRequestHandler):
    def write_bytes(self, content: bytes, content_type: str, status: int = 200):
        self.send_response(status)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(content)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Cross-Origin-Resource-Policy', 'same-origin')
        self.send_header('Content-Security-Policy', "default-src 'self'; img-src 'self' data:; style-src 'self'; script-src 'self'; connect-src 'self'; object-src 'none'; base-uri 'none'")
        self.end_headers()
        self.wfile.write(content)

    def json(self, obj, status=200):
        self.write_bytes(json.dumps(obj,ensure_ascii=False).encode('utf8'), 'application/json; charset=utf-8', status)

    def do_GET(self):
        parsed = urllib.parse.urlsplit(self.path)
        path = parsed.path
        if path == '/api/library':
            self.json(self.server.library)
            return
        if path.startswith('/api/cover/'):
            game_id = urllib.parse.unquote(path[len('/api/cover/'):])
            game = next((g for g in self.server.library.get('games', []) if g.get('id') == game_id), None)
            if not game or not game.get('available') or not game.get('cover_path'):
                self.json({'error':'Cover nicht vorhanden'},404)
                return
            try:
                cover = Path(game['cover_path'])
                if cover.suffix.lower() not in ('.png','.jpg','.jpeg','.webp') or cover.stat().st_size >= 20_000_000:
                    raise OSError('invalid image')
                self.write_bytes(cover.read_bytes(), mimetypes.guess_type(cover.name)[0] or 'application/octet-stream')
            except OSError:
                self.json({'error':'Cover nicht lesbar'},404)
            return
        filename = 'index.html' if path=='/' else path.lstrip('/')
        root=self.server.ui.resolve()
        target=(root / filename).resolve()
        if not target.is_relative_to(root) or not target.is_file():
            self.json({'error':'Nicht gefunden'},404)
            return
        try:
            suffix=mimetypes.guess_type(target.name)[0] or 'application/octet-stream'
            self.write_bytes(target.read_bytes(),suffix+'; charset=utf-8' if suffix.startswith('text/') or suffix=='application/javascript' else suffix)
        except OSError:
            self.json({'error':'Nicht lesbar'},404)

    def do_POST(self):
        if self.path != '/api/refresh':
            self.json({'error':'Nicht unterstützt'},404)
            return
        # Reject cross-site browser requests, even for localhost.
        origin=self.headers.get('Origin','')
        host=self.headers.get('Host','')
        if origin and origin not in ('http://'+host,'https://'+host):
            self.json({'error':'Origin nicht erlaubt'},403)
            return
        try:
            size=int(self.headers.get('Content-Length','0'))
            if size>1024:
                self.json({'error':'Ungültige Anfrage'},413)
                return
        except ValueError:
            self.json({'error':'Ungültige Länge'},400)
            return
        with self.server.lock:
            try:
                result=scan_to_files(self.server.config,self.server.cache,self.server.ui/'library-data.js')
                self.server.library=result
                self.json(result)
            except (OSError,ValueError,TypeError) as exc:
                self.json({'error':type(exc).__name__},500)

    def log_message(self, format, *args):
        if self.path != '/api/library':
            super().log_message(format,*args)


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description='Development-only local UI server')
    parser.add_argument('--port',type=int,default=8765)
    parser.add_argument('--no-initial-scan',action='store_true')
    args=parser.parse_args()
    if not args.no_initial_scan:
        scan_to_files(root/'config/paths.json',root/'data/library-cache.json',root/'ui/library-data.js')
    app=LibraryServer(('127.0.0.1',args.port),Handler,root)
    app.library=load_json(app.cache,{'schema':2,'games':[],'unavailable_paths':[]})
    print(f'Entwicklungsoberfläche: http://127.0.0.1:{args.port}/ (nur auf diesem PC)')
    print('Spielordner: nur LESEN; Cache: data/library-cache.json')
    try:app.serve_forever()
    except KeyboardInterrupt:pass
    finally:app.server_close()

if __name__=='__main__':main()
