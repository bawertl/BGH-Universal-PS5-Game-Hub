"""BGH V1.1.9: PS3 extracted disc folder recovery, without direct-boot claims."""
from pathlib import Path
import socket
import socketserver
import struct
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native_ps5'
PS3_ROOT = '/data/homebrew/PPSA99303/rpcs3/games'
PS3_DIR = PS3_ROOT + '/BLES01181 - Lord of the Rings War of the North'


def ps3_sfo(title='The Lord of the Rings: War in the North', ident='BLES01181'):
    title = title.encode('utf-8') + b'\0'
    ident = ident.encode('ascii') + b'\0'
    keys = b'TITLE\0TITLE_ID\0'
    records = (struct.pack('<HHIII', 0, 0x204, len(title), len(title), 0) +
               struct.pack('<HHIII', 6, 0x204, len(ident), len(ident), len(title)))
    return (b'\0PSF' + struct.pack('<IIII', 0x101, 20 + len(records),
                                   20 + len(records) + len(keys), 2) + records + keys + title + ident)


def compile_cpp(tmp_path, text):
    source = tmp_path / 'probe.cpp'
    binary = tmp_path / 'probe'
    source.write_text(text)
    subprocess.run(['g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                    '-I', str(NATIVE), str(source), str(NATIVE/'library_core.cpp'),
                    str(NATIVE/'ftp_scan.cpp'), str(NATIVE/'ftp_readonly.cpp'), '-o', str(binary)],
                   check=True, capture_output=True)
    return binary


def test_ps3_disc_folder_recognized_and_kept_in_catalogue(tmp_path):
    root = tmp_path / 'games'
    game = root / 'BLES01181 - Lord of the Rings War of the North'
    (game/'PS3_GAME'/'USRDIR').mkdir(parents=True)
    (game/'PS3_GAME'/'PARAM.SFO').write_bytes(ps3_sfo())
    (game/'PS3_GAME'/'ICON0.PNG').write_bytes(b'not-a-real-png')
    (game/'PS3_UPDATE').mkdir()
    (root/'unrelated-folder'/'PS3_GAME').mkdir(parents=True)
    (root/'Random ISO.iso').write_bytes(b'dummy')
    cpp = r'''
#include "library_core.hpp"
#include <cassert>
#include <string>
int main(int argc, char **argv){
    assert(argc==3);
    auto root=ulg::Root{"PS3",argv[1],true};
    auto games=ulg::scan({root});
    ulg::clean_catalogue(games,{root});
    assert(games.games.size()==2);
    bool folder=false,iso=false;
    for(const auto &game:games.games){
        assert(game.platform=="PS3");
        if(game.format=="ps3_ordner"){
            folder=true;
            assert(game.title=="The Lord of the Rings: War in the North");
            assert(game.title_id=="BLES01181");
            assert(game.path==std::string(argv[2]));
            assert(game.cover_path==game.path+"/PS3_GAME/ICON0.PNG");
        }else if(game.format=="iso")iso=true;
    }
    assert(folder && iso);
}
'''
    binary=compile_cpp(tmp_path,cpp)
    subprocess.run([str(binary),str(root),str(game)],check=True,timeout=15)


def test_ps3_ftp_recovers_folder_hidden_by_sandbox(tmp_path):
    sfo=ps3_sfo()
    commands=[]
    listing={
        PS3_ROOT: b'type=dir; BLES01181 - Lord of the Rings War of the North\r\n',
        PS3_DIR: b'type=dir; PS3_GAME\r\ntype=dir; PS3_UPDATE\r\n',
        PS3_DIR+'/PS3_GAME': b'type=file; PARAM.SFO\r\ntype=file; ICON0.PNG\r\n',
    }
    files={PS3_DIR+'/PS3_GAME/PARAM.SFO':sfo}

    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            self.request.settimeout(8)
            stream=self.request.makefile('rb')
            listener=None
            current='/'
            self.request.sendall(b'220 test FTP\r\n')
            try:
                while line:=stream.readline():
                    command=line.decode(errors='replace').strip()
                    commands.append(command)
                    verb=command.split(' ',1)[0]
                    if verb=='USER':self.request.sendall(b'230 logged in\r\n')
                    elif verb=='TYPE':self.request.sendall(b'200 ok\r\n')
                    elif verb=='CWD':
                        path=command[4:]
                        if path in listing:
                            current=path
                            self.request.sendall(b'250 ok\r\n')
                        else:self.request.sendall(b'550 no dir\r\n')
                    elif verb=='PASV':
                        listener=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
                        listener.bind(('127.0.0.1',0));listener.listen(1)
                        port=listener.getsockname()[1]
                        self.request.sendall(f'227 Entering Passive Mode (127,0,0,1,{port//256},{port%256})\r\n'.encode())
                    elif verb in ('MLSD','RETR'):
                        if verb=='RETR' and command[5:] not in files:
                            self.request.sendall(b'550 unavailable\r\n')
                            listener.close();listener=None
                            continue
                        self.request.sendall(b'150 opening\r\n')
                        sock,_=listener.accept()
                        with sock:
                            sock.sendall(listing.get(current,b'') if verb=='MLSD' else files[command[5:]])
                        listener.close();listener=None
                        self.request.sendall(b'226 done\r\n')
                    else:self.request.sendall(b'500 no mutation\r\n')
            finally:
                stream.close()
                if listener:listener.close()

    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address=True
        daemon_threads=True

    code=r'''
#include "library_core.hpp"
#include "ftp_scan.hpp"
#include <cassert>
#include <cstdlib>
#include <string>
int main(int argc,char **argv){
    assert(argc==2);
    const ulg::Root root{"PS3","/data/homebrew/PPSA99303/rpcs3/games",true};
    auto cat=ulg::scan({root});
    assert(cat.games.empty());
    ulg::supplement_from_local_ftp(cat,{root},5,5000,std::atoi(argv[1]));
    ulg::clean_catalogue(cat,{root});
    assert(cat.games.size()==1);
    const auto &g=cat.games.front();
    assert(g.platform=="PS3" && g.format=="ps3_ordner");
    assert(g.via_ftp && g.available);
    assert(g.title_id=="BLES01181");
    assert(g.title=="The Lord of the Rings: War in the North");
    assert(g.path=="/data/homebrew/PPSA99303/rpcs3/games/BLES01181 - Lord of the Rings War of the North");
}
'''
    binary=compile_cpp(tmp_path,code)
    with Server(('127.0.0.1',0), Handler) as server:
        thread=threading.Thread(target=server.serve_forever,daemon=True)
        thread.start()
        try:
            subprocess.run([str(binary),str(server.server_address[1])],check=True,timeout=25,
                           capture_output=True)
        finally:
            server.shutdown();thread.join(timeout=4)
    assert any(cmd=='RETR '+PS3_DIR+'/PS3_GAME/PARAM.SFO' for cmd in commands)
    assert not any(cmd.split(' ')[0] in {'STOR','DELE','RMD','MKD','SITE'} for cmd in commands)


def test_missing_entries_label_is_ascii_safe():
    cpp=(NATIVE/'universal_launcher.cpp').read_text()
    loc=(NATIVE/'ui_localization.hpp').read_text()
    assert '"Keine Eintraege"' in cpp
    assert '{"Keine Eintraege","No entries"}' in loc
