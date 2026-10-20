"""Host tests: exact emulator formats/IDs and write isolation (no PS5 access)."""
from __future__ import annotations
import hashlib
import subprocess
import struct
from pathlib import Path
from PIL import Image
import pytest

ROOT = Path(__file__).resolve().parents[1]

@pytest.fixture(scope='module')
def tool(tmp_path_factory):
    output=tmp_path_factory.mktemp('v08-cli')/'emulator-import'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
        str(ROOT/'native_ps5/emulator_cover_test_cli.cpp'),
        str(ROOT/'native_ps5/emulator_covers.cpp'),
        str(ROOT/'native_ps5/ftp_readonly.cpp'),'-o',str(output)],check=True)
    return output

def call(tool,platform,rom,uid,serial,sources,dest,coverpath=''):
    r=subprocess.run([str(tool),'import',platform,str(rom),uid,serial,str(sources),str(dest),
                      '2121',str(coverpath)],capture_output=True,text=True)
    return r

def tga(path): Image.new('RGBA',(24,30),(20,210,120,255)).save(path,'TGA')
def jpg(path): Image.new('RGB',(45,60),(90,10,210)).save(path,'JPEG')
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def test_prosperoeden_uses_exact_filename_hash_and_private_cache(tool,tmp_path):
    source=tmp_path/'eden';source.mkdir();dest=tmp_path/'private';dest.mkdir()
    rom=tmp_path/'roms'/'Super Game.xci'
    expected=format(fnv1a(rom.name),'08x')+'.tga'
    out=subprocess.check_output([str(tool),'hash',str(rom)],text=True).strip()
    assert out==expected
    image=source/out;tga(image);before=sha(image)
    one=call(tool,'Switch',rom,'rom123','',source,dest)
    assert one.returncode==0,one.stderr
    assert 'FOUND|' in one.stdout and (dest/'rom123.tga').is_file()
    assert sha(image)==before
    image.unlink()  # second start must load own persistent cache
    two=call(tool,'Switch',rom,'rom123','',source,dest)
    assert two.returncode==0 and 'FOUND|' in two.stdout

def fnv1a(text):
    value=2166136261
    for c in text.encode():value=((value^c)*16777619)&0xffffffff
    return value

def test_ps2_manual_and_serial_cover_and_no_game_write(tool,tmp_path):
    source=tmp_path/'ps2';source.mkdir();dest=tmp_path/'cache';dest.mkdir()
    iso=tmp_path/'ps2game.iso'
    make_ps2_iso(iso)
    art=source/'SLUS-21351.jpg';jpg(art);before=sha(art)
    a=call(tool,'PS2',iso,'game-a','',source,dest)
    assert a.returncode==0,a.stdout
    assert (dest/'game-a.jpg').is_file() and sha(art)==before

def make_ps2_iso(path):
    data=bytearray(22*2048)
    header=16*2048;data[header]=1;data[header+1:header+6]=b'CD001'
    root=header+156;data[root]=34
    struct.pack_into('<I',data,root+2,20)
    struct.pack_into('<I',data,root+10,2048)
    entry=20*2048;name=b'SYSTEM.CNF;1'
    data[entry]=33+len(name)
    struct.pack_into('<I',data,entry+2,21)
    content=b'BOOT2 = cdrom0:\\SLUS_213.51;1\r\n'
    struct.pack_into('<I',data,entry+10,len(content))
    data[entry+32]=len(name);data[entry+33:entry+33+len(name)]=name
    data[21*2048:21*2048+len(content)]=content
    path.write_bytes(data)

def make_xex(title):
    data=bytearray(512*1024)
    data[:4]=b'XEX2'
    struct.pack_into('>I',data,0x14,1)
    struct.pack_into('>I',data,0x18,0x00040006)
    struct.pack_into('>I',data,0x1c,0x100)
    struct.pack_into('>I',data,0x10c,title)
    return data

def test_xbox360_title_id_extracted_from_xex_not_filename(tool,tmp_path):
    source=tmp_path/'xbox';source.mkdir();dest=tmp_path/'bg';dest.mkdir()
    xex=tmp_path/'default.xex';xex.write_bytes(make_xex(0x4D5307E6))
    art=source/'4D5307E6.jpg';jpg(art)
    result=call(tool,'Xbox 360',xex,'xbox-a','',source,dest)
    assert result.returncode==0,result.stdout
    assert (dest/'xbox-a.jpg').read_bytes()==art.read_bytes()

def test_xbox360_iso_volume_and_default_xex_title_id(tool,tmp_path):
    source=tmp_path/'xbox';source.mkdir();dest=tmp_path/'bg';dest.mkdir()
    iso=tmp_path/'game.iso';data=bytearray(310*2048)
    offset=32*2048;data[offset:offset+20]=b'MICROSOFT*XBOX*MEDIA'
    struct.pack_into('<I',data,offset+20,33)
    struct.pack_into('<I',data,offset+24,2048)
    entry=33*2048
    struct.pack_into('<I',data,entry+4,40)
    name=b'default.xex';data[entry+13]=len(name)
    data[entry+14:entry+14+len(name)]=name
    xex=make_xex(0x53450815)
    data[40*2048:40*2048+len(xex)]=xex
    iso.write_bytes(data)
    jpg(source/'53450815.jpg')
    result=call(tool,'Xbox 360',iso,'xboxiso','',source,dest)
    assert result.returncode==0,result.stdout
    assert (dest/'xboxiso.jpg').is_file()

def test_corrupt_images_and_unknown_ids_not_imported(tool,tmp_path):
    source=tmp_path/'source';source.mkdir();dest=tmp_path/'bg';dest.mkdir()
    rom=tmp_path/'game.nsp'
    cover=source/(format(fnv1a(rom.name),'08x')+'.tga')
    cover.write_bytes(b'garbage that is not an image')
    result=call(tool,'Switch',rom,'nomatch','',source,dest)
    assert result.returncode==1
    assert not list(dest.iterdir())
    iso=tmp_path/'mygame.iso';iso.write_bytes(b'invalid')
    result=call(tool,'Xbox 360',iso,'unknown','',source,dest)
    assert result.returncode==1 and not list(dest.iterdir())

def test_existing_cover_not_overwritten(tool,tmp_path):
    source=tmp_path/'source';source.mkdir();dest=tmp_path/'bg';dest.mkdir()
    cover=tmp_path/'already.png';cover.write_bytes(b'kept')
    result=call(tool,'PS2',tmp_path/'any.iso','abc','',source,dest,cover)
    assert result.returncode==0 and 'SKIP|' in result.stdout
    assert cover.read_bytes()==b'kept' and not list(dest.iterdir())

def test_ps2_iso_serial_reads_only_bounded_ftp_ranges(tool,tmp_path):
    """For remote ISOs, never transfer the full disc or send STOR/DELE/MKD."""
    import socket
    import socketserver
    import threading
    source=tmp_path/'ps2';source.mkdir();dest=tmp_path/'private';dest.mkdir()
    iso=tmp_path/'physical.iso';make_ps2_iso(iso)
    # Simulate a much larger ISO while only serving the bounded range requests.
    payload=iso.read_bytes()+b'\0'*3000000
    jpg(source/'SLUS-21351.jpg')
    commands=[]

    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            f=self.request.makefile('rb')
            self.request.sendall(b'220 ftp\r\n')
            rest=0
            listener=None
            try:
                while True:
                    line=f.readline().decode('ascii','replace').strip()
                    if not line:break
                    command=line.split()[0].upper();commands.append(command)
                    if command=='USER':reply='230 ok'
                    elif command=='TYPE':reply='200 ok'
                    elif command=='PASV':
                        listener=socket.socket()
                        listener.bind(('127.0.0.1',0));listener.listen(1)
                        port=listener.getsockname()[1]
                        reply=f'227 Entering Passive Mode (127,0,0,1,{port//256},{port%256})'
                    elif command=='REST':
                        rest=int(line.split()[-1]);reply='350 restart accepted'
                    elif command=='RETR':
                        self.request.sendall(b'150 transfer\r\n')
                        listener.settimeout(2)
                        sock,_=listener.accept()
                        try:
                            sock.sendall(payload[rest:])
                        except (BrokenPipeError, ConnectionResetError, OSError):pass
                        sock.close();listener.close();listener=None;rest=0
                        reply='226 done'
                    else:reply='502 not supported'
                    self.request.sendall((reply+'\r\n').encode())
            finally:
                if listener:listener.close()

    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address=True
        daemon_threads=True
    with Server(('127.0.0.1',0),Handler) as server:
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        result=subprocess.run([str(tool),'import','PS2','/remote/Virtual Fighter.iso',
            'ftp-ps2','',str(source),str(dest),str(server.server_address[1]),''],
            text=True,capture_output=True,timeout=15)
        server.shutdown();thread.join(timeout=5)
    assert result.returncode==0,result.stdout+result.stderr
    assert (dest/'ftp-ps2.jpg').is_file()
    assert 'REST' in commands
    assert set(commands).issubset({'USER','TYPE','PASV','REST','RETR'})
