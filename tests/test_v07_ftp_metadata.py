"""V0.7 bounded read-only FTP metadata and PNG cover cache integration."""
from pathlib import Path
import base64
import socket
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native_ps5'
PNG = base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/yEsAAAAASUVORK5CYII=')


def test_ftp_reads_native_title_and_caches_cover_without_game_write(tmp_path):
    source = tmp_path / 'harness.cpp'
    source.write_text('''#include "ftp_scan.hpp"
#include <sys/stat.h>
#include <iostream>
int main(int argc, char **argv){
   if(argc!=3)return 2;
   std::vector<ulg::Root> roots{{"PS5/PS4","/does-not-exist"}};
   auto cat=ulg::scan(roots);
   ulg::supplement_from_local_ftp(cat,roots,3,200,atoi(argv[1]),argv[2]);
   if(cat.games.size()!=1)return 3;
   const auto &g=cat.games[0];
   if(g.title!="FTP Testgame" || g.platform!="PS5" || g.title_id!="PPSA12345")return 4;
   struct stat st{};
   if(g.cover_path.empty() || stat(g.cover_path.c_str(),&st)!=0 || st.st_size<20)return 5;
   std::cout<<g.title<<"|"<<g.cover_path;
   return 0;
}
''')
    exe = tmp_path / 'harness'
    subprocess.run(['g++', '-std=c++20', '-I', str(NATIVE), str(source),
                    str(NATIVE/'library_core.cpp'), str(NATIVE/'ftp_scan.cpp'),
                    str(NATIVE/'ftp_readonly.cpp'), '-o', str(exe)], check=True)
    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    listener.settimeout(10)
    port = listener.getsockname()[1]
    commands = []
    failure = []

    def serve():
        try:
            conn, _ = listener.accept()
            with conn:
                conn.settimeout(8)
                stream = conn.makefile('rwb', buffering=0)
                def reply(msg):
                    stream.write((msg+'\r\n').encode())
                reply('220 Mock FTP')
                cwd = ''
                pasv = None
                while True:
                    raw = stream.readline()
                    if not raw:
                        break
                    cmd = raw.decode().strip()
                    commands.append(cmd)
                    if cmd.startswith('USER '):
                        reply('230 OK')
                    elif cmd == 'TYPE I':
                        reply('200 OK')
                    elif cmd.startswith('CWD '):
                        cwd = cmd[4:]
                        reply('250 OK')
                    elif cmd == 'PASV':
                        pasv = socket.socket()
                        pasv.bind(('127.0.0.1', 0))
                        pasv.listen(1)
                        pasv.settimeout(8)
                        p = pasv.getsockname()[1]
                        reply(f'227 PASV (127,0,0,1,{p//256},{p%256})')
                    elif cmd == 'MLSD':
                        if cwd == '/does-not-exist':
                            payload = b'type=dir; PPSA12345\r\n'
                        elif cwd == '/does-not-exist/PPSA12345':
                            payload = b'type=file; eboot.bin\r\ntype=dir; sce_sys\r\n'
                        else:
                            payload = b''
                        reply('150 Ready')
                        sock, _ = pasv.accept()
                        with sock:
                            sock.sendall(payload)
                        pasv.close(); pasv = None
                        reply('226 Done')
                    elif cmd.startswith('RETR '):
                        target = cmd[5:]
                        if target.endswith('/sce_sys/param.json'):
                            data = b'{"titleId":"PPSA12345","localizedParameters":{"en-US":{"titleName":"FTP Testgame"}}}'
                        elif target.endswith('/sce_sys/icon0.png'):
                            data = PNG
                        else:
                            data = None
                        if data is None:
                            reply('550 Missing')
                            pasv.close(); pasv = None
                        else:
                            reply('150 Ready')
                            sock, _ = pasv.accept()
                            with sock:
                                sock.sendall(data)
                            pasv.close(); pasv = None
                            reply('226 Done')
                    else:
                        reply('500 Unexpected command')
                if pasv:
                    pasv.close()
        except Exception as ex:
            failure.append(repr(ex))
        finally:
            listener.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    cache = tmp_path / 'cover_cache'
    result = subprocess.run([str(exe), str(port), str(cache)], text=True, capture_output=True, timeout=30)
    thread.join(8)
    assert not failure, failure
    assert result.returncode == 0, (result.stdout, result.stderr, commands)
    assert result.stdout.startswith('FTP Testgame|')
    assert any(cmd.startswith('RETR ') for cmd in commands)
    assert all(not cmd.startswith(('STOR ', 'DELE ', 'RMD ', 'MKD ', 'SITE ')) for cmd in commands)
    assert any(cache.iterdir())
