"""V1.1.23: real regression for denied PS5 metadata, direct gallery import, and NLST fallback."""
import io
import socket
import socketserver
import subprocess
import threading
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native_ps5'


def _sample_png(size=(32, 24)):
    img = Image.new('RGBA', size, (18, 87, 156, 255))
    buf = io.BytesIO()
    img.save(buf, format='PNG')
    return buf.getvalue()


def _build(tmp_path, name, content, defines=(), extra=()):
    source = tmp_path / f'{name}.cpp'
    source.write_text(content)
    binary = tmp_path / name
    subprocess.run(
        ['g++', '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror',
         '-I', str(NATIVE), '-I', str(ROOT), '-I', str(ROOT / 'tooling' / 'host_shim'),
         *defines, str(source), *map(str, extra), '-o', str(binary)],
        check=True, timeout=40, capture_output=True, text=True)
    return binary


def test_scan_still_detects_valid_uploaded_image_when_lstat_is_denied(tmp_path):
    root = tmp_path / 'ftp-writable' / 'user_themes'
    background = root / 'backgrounds'
    background.mkdir(parents=True)
    (root / 'logos').mkdir()
    (background / 'Meine Landschaft.png').write_bytes(_sample_png())
    (background / 'Kein Bild.png').write_bytes(b'This is not a PNG')
    (background / '.hidden.png').write_bytes(_sample_png())
    cache = tmp_path / 'private' / 'user_themes'
    cache.parent.mkdir()

    source = '''
#include "native_ps5/bgh_gallery_ftp.hpp"
#include <cassert>
#include <cerrno>
#include <iostream>
#include <sys/stat.h>
extern "C" int __wrap_lstat(const char*, struct stat*) {errno=EPERM; return -1;}
int main(){
    using namespace ulg::bgh_gallery;
    auto before=list(Kind::background);
    assert(before.size()==4);
    assert(valid_filename("Urlaub (1).png"));
    assert(valid_filename("Fruehling & BGH.PNG"));
    assert(!valid_filename("../../escape.png"));
    assert(!valid_filename("eins/zwei.png"));
    assert(before[3].id=="user:Meine Landschaft.png");
    auto imported=import_from_local_filesystem(Kind::background);
    assert(imported.readable==1 && imported.cached==1 && imported.cache_errors==0);
    auto after=list(Kind::background);
    assert(after.size()==4 && after[3].id=="user:Meine Landschaft.png");
    assert(after[3].path.find("private")!=std::string::npos);
    Selection sel;
    assert(select(Kind::background,"user:Meine Landschaft.png",sel));
    std::cout<<"DIRECT_OK";
}
'''
    executable = _build(tmp_path, 'lstat-denied', source, defines=[
        f'-DBGH_GALLERY_UPLOAD_ROOT="{root}"',
        f'-DBGH_GALLERY_CACHE_ROOT="{cache}"',
        '-Wl,--wrap=lstat'])
    result = subprocess.run([str(executable)], check=True, capture_output=True, text=True, timeout=15)
    assert 'DIRECT_OK' in result.stdout
    assert (cache / 'backgrounds' / 'Meine Landschaft.png').read_bytes() == _sample_png()


def test_gallery_import_falls_back_from_unsupported_mlsd_to_nlst(tmp_path):
    cache = tmp_path / 'gallery-cache'
    data = _sample_png()
    sent_commands = []

    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            self.request.settimeout(5)
            reader = self.request.makefile('rb')
            passive = None
            self.request.sendall(b'220 PS5 simulated FTP\r\n')
            try:
                while line := reader.readline():
                    command = line.decode().strip()
                    sent_commands.append(command)
                    opcode = command.split(' ', 1)[0]
                    if opcode == 'USER':
                        self.request.sendall(b'230 logged in\r\n')
                    elif opcode == 'TYPE':
                        self.request.sendall(b'200 binary mode\r\n')
                    elif opcode == 'CWD':
                        self.request.sendall(b'250 folder exists\r\n')
                    elif opcode == 'PASV':
                        passive = socket.socket()
                        passive.bind(('127.0.0.1', 0))
                        passive.listen(1)
                        port = passive.getsockname()[1]
                        self.request.sendall(f'227 (127,0,0,1,{port//256},{port%256})\r\n'.encode())
                    elif opcode == 'MLSD':
                        self.request.sendall(b'502 MLSD unsupported\r\n')
                        passive.close()
                        passive = None
                    elif opcode in ('NLST', 'RETR'):
                        self.request.sendall(b'150 starting transfer\r\n')
                        conn, _ = passive.accept()
                        with conn:
                            conn.sendall(b'Mein Bild.png\r\n' if opcode == 'NLST' else data)
                        passive.close()
                        passive = None
                        self.request.sendall(b'226 done\r\n')
                    else:
                        self.request.sendall(b'500 unknown\r\n')
            finally:
                reader.close()
                if passive:
                    passive.close()

    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True

    source = '''
#include "native_ps5/bgh_gallery_ftp.hpp"
#include <cassert>
#include <iostream>
int main(int /*argc*/,char**argv){
    using namespace ulg::bgh_gallery;
    auto r=import_from_local_ftp(Kind::background,8,std::stoi(argv[1]));
    assert(r.ftp_ok && r.added==1 && r.failed==0);
    auto pics=list(Kind::background);
    assert(pics.size()==4);
    assert(pics[3].id=="user:Mein Bild.png");
    std::cout<<"NLST_OK";
}
'''
    exe = _build(tmp_path, 'nlst-fallback', source, defines=[
        f'-DBGH_GALLERY_CACHE_ROOT="{cache}"'], extra=[NATIVE / 'ftp_readonly.cpp'])
    with Server(('127.0.0.1', 0), Handler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            result = subprocess.run([str(exe), str(server.server_address[1])],
                                    check=True, capture_output=True, text=True, timeout=15)
            assert 'NLST_OK' in result.stdout
        finally:
            server.shutdown()
            thread.join(timeout=3)
    assert any(cmd == 'MLSD' for cmd in sent_commands)
    assert any(cmd == 'NLST' for cmd in sent_commands)
    assert any(cmd.startswith('RETR ') for cmd in sent_commands)
    assert (cache / 'backgrounds' / 'Mein Bild.png').read_bytes() == data


def test_4k_png_decoder_supports_wallpapers_without_unbounded_output(tmp_path):
    path = tmp_path / '4k-wallpaper.png'
    path.write_bytes(_sample_png((3840, 2160)))
    cpp = '''
#include "native_ps5/cover_png.hpp"
#include <cassert>
#include <iostream>
int main(int /*argc*/,char**argv){
    ulg::PngImage image;
    assert(ulg::decode_png(argv[1],image));
    assert(image.width>0 && image.height>0);
    assert(image.width<=512 && image.height<=512);
    std::cout<<"4K_OK";
}
'''
    executable = _build(tmp_path, 'decoder-4k', cpp, extra=[NATIVE/'cover_png.cpp', '-lz'])
    result = subprocess.run([str(executable), str(path)], check=True,
                            capture_output=True, text=True, timeout=20)
    assert '4K_OK' in result.stdout


def test_only_gallery_entry_point_uses_file_import_and_ftp_fallback():
    source = (NATIVE / 'universal_launcher.cpp').read_text()
    assert 'import_from_local_filesystem(kind)' in source
    assert 'direct.readable==0 || direct.cache_errors>0' in source
    assert 'import_from_local_ftp(kind)' in source
    assert 'constexpr const char *kBuildLabel = "BGH V1.1.23"' in source
    assert 'lstat(path.c_str()' not in (NATIVE/'bgh_theme_gallery.hpp').read_text()
    for needle in ('controllers_.poll();','ulg::shadow_storage::fetch_storage()',
                   'stage_ps5sx2_disc_launch(g.path)', 'make_registered_ps4_launch_plan(g.title_id)'):
        assert needle in source
