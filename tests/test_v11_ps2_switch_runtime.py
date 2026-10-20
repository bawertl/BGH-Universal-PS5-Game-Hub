"""Simulate PS5 sandbox's inability to open game/cover sources: read-only FTP import."""
from __future__ import annotations
import socket
import socketserver
import subprocess
import threading
from pathlib import Path
from PIL import Image
from test_v08_emulator_covers import fnv1a, make_ps2_iso

ROOT=Path(__file__).resolve().parents[1]


def cli(tmp_path):
    dest=tmp_path/'emulator-import'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
        str(ROOT/'native_ps5/emulator_cover_test_cli.cpp'),
        str(ROOT/'native_ps5/emulator_covers.cpp'),
        str(ROOT/'native_ps5/ftp_readonly.cpp'),'-o',str(dest)],check=True)
    return dest


def jpg_bytes(tmp_path):
    file=tmp_path/'cover.jpg';Image.new('RGB',(128,170),(70,50,240)).save(file,'JPEG')
    return file.read_bytes()


def tga_bytes(tmp_path):
    file=tmp_path/'cover.tga';Image.new('RGBA',(96,128),(20,200,90,255)).save(file,'TGA')
    return file.read_bytes()


class VirtualFtp(socketserver.BaseRequestHandler):
    """Small passive FTP fixture. Purposefully refuses REST to test compatibility."""
    files={}
    commands=[]
    def handle(self):
        reply=self.request.sendall
        reply(b'220 FTP ready\r\n')
        reader=self.request.makefile('rb')
        listener=None
        try:
            while (commandline:=reader.readline()):
                cmd=commandline.decode('utf-8','replace').strip()
                name=cmd.split(' ',1)[0].upper()
                self.commands.append(name)
                if name=='USER':code='230 OK'
                elif name=='TYPE':code='200 Binary'
                elif name=='REST':code='502 REST not supported'
                elif name=='PASV':
                    listener=socket.socket();listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
                    listener.bind(('127.0.0.1',0));listener.listen(1)
                    port=listener.getsockname()[1]
                    code=f'227 Passive Mode (127,0,0,1,{port//256},{port%256})'
                elif name=='RETR':
                    path=cmd[5:]
                    if path not in self.files:
                        code='550 File not found'
                    else:
                        reply(b'150 Starting transfer\r\n')
                        listener.settimeout(2)
                        stream,_=listener.accept()
                        try:stream.sendall(self.files[path])
                        except (OSError,BrokenPipeError,ConnectionResetError):pass
                        stream.close();listener.close();listener=None
                        code='226 OK'
                else: code='502 Unsupported'
                reply((code+'\r\n').encode())
        finally:
            reader.close()
            if listener:listener.close()


class ThreadedFtp(socketserver.ThreadingTCPServer):
    allow_reuse_address=True
    daemon_threads=True


def serve(files):
    class Handler(VirtualFtp):
        pass
    Handler.files=files
    Handler.commands=[]
    server=ThreadedFtp(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True)
    thread.start()
    return server,thread,Handler


def run(exe,platform,rom,uid,sources,dest,port):
    return subprocess.run([str(exe),'import',platform,str(rom),uid,'',str(sources),str(dest),str(port),''],capture_output=True,text=True,timeout=30)


def test_ps2_iso_and_jpg_with_rest_refused_and_no_direct_file_access(tmp_path):
    image=tmp_path/'local.iso';make_ps2_iso(image)
    remote='/mnt/usb1/PS5SX2/games/Ratchet.iso'
    artwork='/data/PCSX2/covers/SLUS-21351.jpg'
    src=jpg_bytes(tmp_path)
    server,thread,handler=serve({remote:image.read_bytes(),artwork:src})
    try:
        dest=tmp_path/'cache';dest.mkdir()
        result=run(cli(tmp_path),'PS2',remote,'ps2-remote','/data/PCSX2/covers',dest,server.server_address[1])
    finally:
        server.shutdown();server.server_close();thread.join(timeout=2)
    assert result.returncode==0,result.stdout+result.stderr
    assert (dest/'ps2-remote.jpg').read_bytes()==src
    assert 'REST' in handler.commands
    assert 'STOR' not in handler.commands and 'DELE' not in handler.commands


def test_prosperoeden_tga_import_over_ftp_uses_rom_filename_not_path(tmp_path):
    path='/mnt/usb1/eden/roms/The Game [0100000011110000].nsp'
    image_name=f'{fnv1a(Path(path).name):08x}'
    art='/data/prosperoeden/covers/'+image_name+'.tga'
    data=tga_bytes(tmp_path)
    server,thread,handler=serve({art:data, '/data/prosperoeden/covers/'+image_name+'.name':b'The Game'})
    try:
        dest=tmp_path/'cache';dest.mkdir()
        result=run(cli(tmp_path),'Switch',path,'switch-remote','/data/prosperoeden/covers',dest,server.server_address[1])
    finally:
        server.shutdown();server.server_close();thread.join(timeout=2)
    assert result.returncode==0,result.stdout+result.stderr
    assert (dest/'switch-remote.tga').read_bytes()==data
    assert 'STOR' not in handler.commands


def test_ps2_cached_chd_serial_works_without_native_stat(tmp_path):
    src=tmp_path/'ps2';src.mkdir()
    dest=tmp_path/'private';dest.mkdir()
    remote='/mnt/usb1/PS5SX2/games/virtualfighter4.chd'
    (src/'chd-serials.txt').write_text(f'{remote}\t1288800\t10000\tSLUS-21351\n')
    data=jpg_bytes(tmp_path);(src/'SLUS-21351.jpg').write_bytes(data)
    r=run(cli(tmp_path),'PS2',remote,'chd-remote',src,dest,1)
    assert r.returncode==0,r.stdout+r.stderr
    assert (dest/'chd-remote.jpg').read_bytes()==data


def test_missing_iso_serial_has_specific_diagnostic(tmp_path):
    src=tmp_path/'source';src.mkdir()
    dest=tmp_path/'private';dest.mkdir()
    remote='/mnt/usb1/PS5SX2/games/missing.iso'
    r=run(cli(tmp_path),'PS2',remote,'missing',src,dest,1)
    assert r.returncode==1
    assert 'SERIENNUMMER FEHLT' in r.stdout


def test_ps2_usb_root_cover_found_even_if_mlsd_and_rest_unsupported(tmp_path):
    image=tmp_path/'local.iso';make_ps2_iso(image)
    remote='/mnt/usb1/PS5SX2/games/Example.iso'
    art='/mnt/usb1/covers/SLUS-21351.jpg'
    data=jpg_bytes(tmp_path)
    server,thread,handler=serve({remote:image.read_bytes(),art:data})
    try:
        dest=tmp_path/'cache';dest.mkdir()
        result=run(cli(tmp_path),'PS2',remote,'ps2-usb','/data/PCSX2/covers',dest,server.server_address[1])
    finally:
        server.shutdown();server.server_close();thread.join(timeout=2)
    assert result.returncode==0,result.stdout+result.stderr
    assert (dest/'ps2-usb.jpg').read_bytes()==data
