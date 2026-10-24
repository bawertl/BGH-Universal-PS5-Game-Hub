"""Real loopback FTP -> private BGH image-cache regression, no PS5 needed."""
import io
import shutil
import socket
import socketserver
import subprocess
import threading
from pathlib import Path

from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
NATIVE=ROOT/'native_ps5'

def test_gallery_imports_from_ftp_when_app_cannot_open_remote_folder(tmp_path):
    cache=tmp_path/'private-gallery'
    test_cpp=tmp_path/'test-gallery.cpp'
    include=tmp_path/'includes'/'concepts'
    include.mkdir(parents=True)
    shutil.copyfile(NATIVE/'ftp_readonly.hpp',include/'ftp_readonly.hpp')
    test_cpp.write_text('''
#include "concepts/ftp_readonly.hpp"
#include "native_ps5/bgh_gallery_ftp.hpp"
#include <cassert>
#include <iostream>
int main(int argc,char**argv){
    const int port=std::stoi(argv[1]);
    const auto before=ulg::bgh_gallery::list(ulg::bgh_gallery::Kind::background);
    assert(before.size()==3);
    const auto report=ulg::bgh_gallery::import_from_local_ftp(ulg::bgh_gallery::Kind::background,8,port);
    assert(report.ftp_ok && report.added==1 && report.failed==0);
    const auto after=ulg::bgh_gallery::list(ulg::bgh_gallery::Kind::background);
    assert(after.size()==4);
    assert(after.back().id=="user:Mein Bild.png");
    const auto twice=ulg::bgh_gallery::import_from_local_ftp(ulg::bgh_gallery::Kind::background,8,port);
    assert(twice.ftp_ok && twice.added==0 && twice.already==1);
    std::cout<<"BGH_GALLERY_FTP_OK";
}
''')
    exe=tmp_path/'gallery-test'
    subprocess.run(['g++','-std=c++17','-O1','-I',str(ROOT),
        '-I',str(tmp_path/'includes'),'-I',str(NATIVE),'-I',str(ROOT/'tooling/host_shim'),
        f'-DBGH_GALLERY_CACHE_ROOT="{cache}"',
        str(test_cpp),str(NATIVE/'ftp_readonly.cpp'),'-o',str(exe)],
        check=True,capture_output=True,text=True,timeout=30)
    art=Image.new('RGBA',(48,48),(20,120,230,255))
    with io.BytesIO() as buf:
        art.save(buf,format='PNG')
        valid_png=buf.getvalue()
    commands=[]
    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            self.request.settimeout(10)
            reader=self.request.makefile('rb')
            passive=None
            self.request.sendall(b'220 BGH FTP test\r\n')
            try:
                while cmd:=reader.readline():
                    cmd=cmd.decode('utf-8').strip();commands.append(cmd)
                    op=cmd.split(' ',1)[0]
                    if op=='USER': self.request.sendall(b'230 ok\r\n')
                    elif op=='TYPE': self.request.sendall(b'200 ok\r\n')
                    elif op=='CWD':
                        path=cmd[4:]
                        target='/data/homebrew/PPSA99079/user_themes/backgrounds'
                        self.request.sendall(b'250 ok\r\n' if path==target else b'550 no\r\n')
                    elif op=='PASV':
                        passive=socket.socket();passive.bind(('127.0.0.1',0));passive.listen(1)
                        port=passive.getsockname()[1]
                        self.request.sendall(f'227 Entering Passive Mode (127,0,0,1,{port//256},{port%256})\r\n'.encode())
                    elif op in ('MLSD','RETR'):
                        if op=='RETR' and cmd[5:]!='/data/homebrew/PPSA99079/user_themes/backgrounds/Mein Bild.png':
                            self.request.sendall(b'550 no\r\n');passive.close();passive=None;continue
                        self.request.sendall(b'150 go\r\n')
                        conn,_=passive.accept()
                        with conn:
                            conn.sendall(b'type=file; Mein Bild.png\r\ntype=file; .keep\r\n' if op=='MLSD' else valid_png)
                        passive.close();passive=None
                        self.request.sendall(b'226 done\r\n')
                    else: self.request.sendall(b'500 not supported\r\n')
            finally:
                reader.close()
                if passive:passive.close()
    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address=True
        daemon_threads=True
    with Server(('127.0.0.1',0),Handler) as server:
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        try:
            r=subprocess.run([str(exe),str(server.server_address[1])],
                check=True,text=True,capture_output=True,timeout=20)
            assert 'BGH_GALLERY_FTP_OK' in r.stdout
            assert any(c.startswith('MLSD') for c in commands)
            assert commands.count('RETR /data/homebrew/PPSA99079/user_themes/backgrounds/Mein Bild.png')==1
        finally:
            server.shutdown();thread.join(timeout=3)
    assert (cache/'backgrounds'/'Mein Bild.png').read_bytes()==valid_png
    assert not (cache/'backgrounds'/'..').is_file()
    assert all(not c.startswith(('DELE','STOR','MKD','RMD')) for c in commands)


def test_no_network_during_per_frame_gallery_render():
    ui=(NATIVE/'universal_launcher.cpp').read_text()
    # Network import is only triggered when opening a gallery tile.
    assert 'const auto ftp=ulg::bgh_gallery::import_from_local_ftp(kind);' in ui
    render=ui.split('void draw_settings_gallery_detail')[1].split('void draw_root_button_legend')[0]
    assert 'import_from_local_ftp' not in render
    assert 'Bilder: /data/homebrew/PPSA99079/user_themes/backgrounds' in render
