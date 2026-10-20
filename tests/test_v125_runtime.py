"""Regression checks for new sandbox discovery and explicit emulator menu launches."""
from pathlib import Path
import socket
import socketserver
import subprocess
import threading
import pytest
from PIL import Image
from test_v11_ps2_switch_runtime import cli
ROOT=Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("source",["covers","library"])
def test_mounted_xbox_sandbox_import_is_readonly(tmp_path,source):
    suffix='.jpg' if source=='covers' else '.png'
    art=tmp_path/('sample'+suffix);Image.new('RGB',(48,64),(30,150,100)).save(art)
    data=art.read_bytes()
    h=1469598103934665603
    for c in b'/games/Gears.iso':h=((h^c)*1099511628211)&0xffffffffffffffff
    name='4D53082D.jpg' if source=='covers' else f'{h:016x}.png'
    remote=f'/mnt/sandbox/PPSA50011_abc123/download0/xbox360ps5/{source}/{name}' 
    commands=[]
    class Handler(socketserver.StreamRequestHandler):
        def handle(self):
            cwd='';pasv=None
            def reply(s):self.wfile.write((s+'\r\n').encode())
            def transfer(payload):
                reply('150 Data')
                sock,_=pasv.accept()
                with sock:sock.sendall(payload)
                pasv.close()
                reply('226 Done')
            reply('220 Fixture')
            try:
                while line:=self.rfile.readline():
                    cmd=line.decode().strip();commands.append(cmd)
                    if cmd.startswith('USER '):reply('230 OK')
                    elif cmd=='TYPE I':reply('200 OK')
                    elif cmd.startswith('CWD '):cwd=cmd[4:];reply('250 OK')
                    elif cmd=='PASV':
                        pasv=socket.socket();pasv.bind(('127.0.0.1',0));pasv.listen(1);pasv.settimeout(3)
                        p=pasv.getsockname()[1];reply(f'227 (127,0,0,1,{p//256},{p%256})')
                    elif cmd=='MLSD':
                        payload=(b'type=dir; PPSA50011_abc123\r\ntype=dir; PPSA99999_other\r\n'
                                 if cwd=='/mnt/sandbox' else
                                 f'type=file; {name}\r\n'.encode() if cwd==str(Path(remote).parent) else b'')
                        transfer(payload);pasv=None
                    elif cmd.startswith('SIZE '):reply(f'213 {len(data)}' if cmd[5:]==remote else '550 Missing')
                    elif cmd.startswith('RETR '):
                        if cmd[5:]==remote:transfer(data);pasv=None
                        else:reply('550 Missing')
                    else:reply('502 Unsupported')
            finally:
                if pasv:pasv.close()
    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address=True;daemon_threads=True
    server=Server(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        dest=tmp_path/'cache';dest.mkdir()
        p=subprocess.run([str(cli(tmp_path)),'import','Xbox 360','/games/Gears.iso','gears','4D53082D','/unavailable',str(dest),str(server.server_address[1]),''],text=True,capture_output=True,timeout=30)
    finally:
        server.shutdown();server.server_close();thread.join(3)
    assert p.returncode==0,p.stdout+p.stderr
    assert (dest/('gears'+suffix)).read_bytes()==data
    assert f'RETR {remote}' in commands
    assert not any(c.split()[0] in {'STOR','DELE','MKD','RMD'} for c in commands)
    assert not any('PPSA99999_other/' in c for c in commands)


def test_native_launch_reports_actual_result_and_menu_has_no_rom(tmp_path):
    cpp=tmp_path/'launch.cpp'
    cpp.write_text(r'''
#include "ps5_launch.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
static int calls=0;
extern "C" int sceUserServiceGetForegroundUser(std::uint32_t* u){*u=7;return 0;}
extern "C" int sceSystemServiceLaunchApp(const char* id,const char** args,void* ctx){
 ++calls;assert(args && args[0]==nullptr);
 const auto* p=static_cast<const std::uint32_t*>(ctx);assert(p[0]==32 && p[1]==7);
 assert(std::strcmp(id,"PPSA99203")==0 || std::strcmp(id,"PPSA21567")==0);
 return static_cast<int>(0x8094000Fu);
}
int main(){
 auto a=ulg::launch_registered_ps5("PPSA21567");assert(!a.accepted);
 assert(a.message.find("8094000F")!=std::string::npos);
 assert(a.message.find("registriert")==std::string::npos);
 auto b=ulg::launch_emulator_menu("PS2");assert(!b.accepted && calls==2);
 ulg::launch_emulator_menu("untrusted");ulg::launch_registered_ps5("PPSA99079");assert(calls==2);
}
''')
    exe=tmp_path/'launch'
    subprocess.run(['g++','-std=c++20','-DBGH_TARGET_PS5','-Wall','-Wextra','-Werror','-I',str(ROOT/'native_ps5'),str(cpp),str(ROOT/'native_ps5/ps5_launch.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_controller_monitor_preserves_ui_handle_and_reports_unknown(tmp_path):
    exe=tmp_path/'controller'
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-I',str(ROOT/'native_ps5'),str(ROOT/'tests/controller_monitor_harness.cpp'),'-Wl,--export-dynamic','-ldl','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
