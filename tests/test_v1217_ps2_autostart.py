"""BGH V0.12.18: FTP-isolated PS5SX2 request, mocked localhost daemon."""
from pathlib import Path
import hashlib
import socket
import subprocess
import threading
import pytest

ROOT=Path(__file__).resolve().parents[1]
NATIVE=ROOT/'native_ps5'
GAME='/mnt/usb1/PS5SX2/games/Baldur\'s Gate - Dark Alliance II (Europe, Australia) (En,Fr,De).iso'

@pytest.fixture(scope='module')
def helper(tmp_path_factory):
    location=tmp_path_factory.mktemp('ps2-ftp-stage')
    src=location/'main.cpp'
    src.write_text(r'''
#include "ps2_autostart.hpp"
#include <iostream>
#include <string>
int main(int argc,char**argv){
    if(argc<3)return 1;
    std::string game=argv[1];
    int port=std::stoi(argv[2]);
    std::string path=argc>=4?argv[3]:ulg::kPs2DiscLaunchPath;
    auto ticket=ulg::stage_ps5sx2_disc_launch(game,path,port);
    std::cout<<(ticket.ready?1:0)<<"|"<<ticket.ftp_reply<<"|"<<ticket.os_error<<"|"<<ticket.message<<"\n";
    if(argc>=5 && std::string(argv[4])=="cancel")
        std::cout<<"cancel="<<(ulg::cancel_ps5sx2_disc_launch(ticket,port)?1:0)<<"\n";
    return 0;
}
''')
    out=location/'ps2-ftp-cli'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',f'-I{NATIVE}',
                    str(src),str(NATIVE/'ftp_ps2_writer.cpp'),str(NATIVE/'ftp_readonly.cpp'),'-o',str(out)],
                   check=True,capture_output=True,text=True)
    return out

class FTPMock:
    def __init__(self, initial=None, reject_stor=False, corrupt_retr=False, deny_listing=False, reject_existing=False):
        self.files=dict(initial or {})
        self.reject_stor=reject_stor
        self.reject_existing=reject_existing
        self.corrupt_retr=corrupt_retr
        self.deny_listing=deny_listing
        self.commands=[]
        self.errors=[]
        self.active=[]
        self.stop=threading.Event()
        self.server=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
        self.server.bind(('127.0.0.1',0))
        self.server.listen(8)
        self.server.settimeout(.2)
        self.port=self.server.getsockname()[1]
        self.thread=threading.Thread(target=self._serve,daemon=True)
    def __enter__(self):
        self.thread.start()
        return self
    def __exit__(self,*args):
        self.stop.set()
        self.server.close()
        self.thread.join(4)
        for t in self.active:t.join(4)
        assert not self.errors,self.errors

    def _serve(self):
        while not self.stop.is_set():
            try:
                control,_=self.server.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            t=threading.Thread(target=self._session,args=(control,),daemon=True)
            self.active.append(t)
            t.start()

    def _session(self,control):
        passive=None
        try:
            with control:
                control.settimeout(3)
                f=control.makefile('rwb',buffering=0)
                def answer(s):f.write((s+'\r\n').encode())
                answer('220 Welcome')
                while not self.stop.is_set():
                    raw=f.readline()
                    if not raw:break
                    cmd=raw.decode('utf-8').strip()
                    self.commands.append(cmd)
                    if cmd=='USER anonymous':answer('230 Ready')
                    elif cmd=='TYPE I':answer('200 Binary')
                    elif cmd=='CWD /data/PCSX2/logs':answer('250 OK')
                    elif cmd=='PASV':
                        passive=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
                        passive.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
                        passive.bind(('127.0.0.1',0))
                        passive.listen(1)
                        passive.settimeout(2)
                        port=passive.getsockname()[1]
                        answer(f'227 Entering Passive Mode (192,168,1,75,{port//256},{port%256})')
                    elif cmd=='MLSD':
                        if self.deny_listing:
                            answer('550 Not permitted')
                            if passive:passive.close();passive=None
                            continue
                        answer('150 Data')
                        data,_=passive.accept()
                        with data:
                            for name,value in sorted(self.files.items()):
                                data.sendall(f'type=file;size={len(value)}; {name}\r\n'.encode())
                        passive.close();passive=None
                        answer('226 Done')
                    elif cmd=='STOR disc-launch.txt':
                        if self.reject_stor or (self.reject_existing and 'disc-launch.txt' in self.files):
                            answer('550 Read-only')
                            if passive:passive.close();passive=None
                            continue
                        answer('150 Data')
                        data,_=passive.accept()
                        with data:
                            data.settimeout(2)
                            body=b''
                            while True:
                                chunk=data.recv(4096)
                                if not chunk:break
                                body+=chunk
                        passive.close();passive=None
                        self.files['disc-launch.txt']=body
                        answer('226 Stored')
                    elif cmd=='RETR /data/PCSX2/logs/disc-launch.txt':
                        if 'disc-launch.txt' not in self.files:
                            answer('550 Missing')
                            if passive:passive.close();passive=None
                            continue
                        answer('150 Data')
                        data,_=passive.accept()
                        with data:
                            value=self.files['disc-launch.txt']
                            data.sendall(value+b'INVALID' if self.corrupt_retr else value)
                        passive.close();passive=None
                        answer('226 Done')
                    elif cmd=='DELE disc-launch.txt':
                        if 'disc-launch.txt' in self.files:
                            self.files.pop('disc-launch.txt')
                            answer('250 Deleted')
                        else:answer('550 Missing')
                    else:answer('500 Unsupported')
        except Exception as e:
            if not self.stop.is_set():self.errors.append(repr(e))
        finally:
            if passive:passive.close()

def call(helper,port,game=GAME,path=None,cancel=False):
    params=[str(helper),game,str(port)]
    if path or cancel:params.append(path or '/data/PCSX2/logs/disc-launch.txt')
    if cancel:params.append('cancel')
    return subprocess.run(params,capture_output=True,text=True,timeout=12,check=True).stdout

def test_ftp_upload_emits_exact_selected_full_path_and_verifies(helper):
    with FTPMock() as server:
        out=call(helper,server.port)
        assert out.startswith('1|226|0|'),out
        assert server.files['disc-launch.txt']==(GAME+'\n').encode()
        assert 'CWD /data/PCSX2/logs' in server.commands
        assert 'STOR disc-launch.txt' in server.commands
        assert 'RETR /data/PCSX2/logs/disc-launch.txt' in server.commands
        assert not any(c.startswith(('DELE','SITE','MKD','RMD')) for c in server.commands)

def test_existing_one_shot_request_is_safely_replaced_and_verified(helper):
    original=b'/mnt/usb1/PS5SX2/games/virtuafighter4.chd\n'
    with FTPMock({'disc-launch.txt':original}) as server:
        out=call(helper,server.port)
        assert out.startswith('1|226|0|'),out
        assert server.files['disc-launch.txt']==(GAME+'\n').encode()
        assert server.commands.count('STOR disc-launch.txt')==1
        assert not any(c.startswith('DELE ') for c in server.commands)

def test_overwrite_refused_fallback_only_deletes_fixed_request(helper):
    old=b'/mnt/usb1/PS5SX2/games/old.iso\n'
    with FTPMock({'disc-launch.txt':old,'unrelated-save.bin':b'preserve'},reject_existing=True) as server:
        out=call(helper,server.port)
        assert out.startswith('1|226|0|'),out
        assert server.files['disc-launch.txt']==(GAME+'\n').encode()
        assert server.files['unrelated-save.bin']==b'preserve'
        assert server.commands.count('DELE disc-launch.txt')==1
        assert server.commands.count('STOR disc-launch.txt')==2
        assert not any(c.startswith(('DELE unrelated','RMD','SITE','MKD')) for c in server.commands)

def test_clean_on_forwarder_failure_if_unchanged(helper):
    with FTPMock() as server:
        out=call(helper,server.port,cancel=True)
        assert 'cancel=1' in out,out
        assert 'disc-launch.txt' not in server.files
        assert 'DELE disc-launch.txt' in server.commands

def test_unknown_or_readonly_ftp_produces_visible_error_no_false_success(helper):
    with FTPMock(reject_stor=True) as server:
        out=call(helper,server.port)
        assert out.startswith('0|550|0|'),out
        assert 'disc-launch.txt' not in server.files
    with FTPMock(deny_listing=True) as server:
        out=call(helper,server.port)
        assert out.startswith('0|550|0|'),out
        assert 'STOR disc-launch.txt' not in server.commands

def test_retrieve_mismatch_blocks_forwarder_even_if_stor_returns_226(helper):
    with FTPMock(corrupt_retr=True) as server:
        out=call(helper,server.port)
        assert out.startswith('0|226|0|'),out
        assert 'nicht unveraendert ruecklesbar' in out

def test_fail_closed_on_bad_game_path_or_wrong_destination(helper):
    with FTPMock() as server:
        for bad in ('', '/etc/passwd', '/mnt/usb9/game.iso', '/mnt/usb1/PS5SX2/games/a.nsp',
                    '/mnt/usb1/PS5SX2/games/../a.iso', '/mnt/usb1/a.iso\nsecond.iso'):
            assert call(helper,server.port,bad).startswith('0|0|22|')
        assert call(helper,server.port,GAME,'/data/PCSX2/disc-launch.txt').startswith('0|0|22|')
        assert not server.commands

def test_missing_ftp_is_reported(helper):
    sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
    assert '0|0|0|' in call(helper,port)

def test_ps5_ps4_launch_protocol_unchanged_except_v127_prelaunch_history():
    src=(NATIVE/'universal_launcher.cpp').read_text()
    # V1.0.5 deliberately moves only history persistence ahead of the handoff.
    for start,end,sha in [
        ('else if(g.platform=="PS5"){','else if(g.platform=="PS4"){','6ac541fa6547caa4d1d0359f90b67fabf8a87ab521acea5c738b592468030a11'),
        ('else if(g.platform=="PS4"){','else if(g.platform=="PS2"){','98fbaa1a84e3a5b1bfb72eb768ebde1a6cddee78fd9a34b7c854c492e49a4102'),
    ]:
        assert hashlib.sha256(src.split(start)[1].split(end)[0].encode()).hexdigest()==sha
    ps2=src.split('else if(g.platform=="PS2"){')[1].split('else if(!plan.supported)')[0]
    assert 'ulg::stage_ps5sx2_disc_launch(g.path)' in ps2
    assert 'ulg::send_launch(ps2_plan)' in ps2
    assert 'ulg::cancel_ps5sx2_disc_launch(request)' in ps2
    assert 'if(bridge.status==ulg::LaunchStatus::queued)sys::quit();' in ps2
    assert 'BGH V1.1.30' in src
    assert 'ftp_stage_ps2_request' in (NATIVE/'ps2_autostart.hpp').read_text()
    assert 'O_CREAT' not in (NATIVE/'ps2_autostart.hpp').read_text()
    overlay=(ROOT/'tooling/install_ui_kit_overlay.py').read_text()
    assert "project/'native_ps5'/'ftp_ps2_writer.cpp'" in overlay
    assert 'bgh-v1.1.30-fix2' in (ROOT/'.github/workflows/ps5-build.yml').read_text()
