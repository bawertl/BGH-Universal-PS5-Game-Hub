"""V0.12.17 PS2 forwarder bridge with the menu fallback; host-only fake TCP daemon, no PS5 modifications."""
import socket
import struct
import subprocess
import threading
from pathlib import Path
import pytest

ROOT=Path(__file__).resolve().parents[1]
CPP=ROOT/'native_ps5'

@pytest.fixture(scope='module')
def cli():
    subprocess.run(['make','-B','-C',str(CPP),'CXX=g++','launch-test-cli'],check=True,capture_output=True,text=True)
    return CPP/'launch-test-cli'

def fake_start(cli,platform,value,stage=3,rc=0x80940010):
    with socket.socket() as server:
        server.bind(('127.0.0.1',0));server.listen(1);server.settimeout(4)
        received=[]
        port=server.getsockname()[1]
        def handle():
            sock,_=server.accept()
            with sock:
                buf=b''
                while len(buf)<0x1020:
                    chunk=sock.recv(0x1020-len(buf))
                    if not chunk:break
                    buf+=chunk
                received.append(buf)
                sock.sendall(struct.pack('<6I',0x4C574650,1,stage,rc,rc,17))
        thread=threading.Thread(target=handle,daemon=True);thread.start()
        proc=subprocess.run([str(cli),platform,value,str(port)],capture_output=True,text=True,timeout=5)
        thread.join(timeout=4)
        assert proc.returncode==0 and len(received)==1
        return proc.stdout,received[0]

def test_ps2_uses_ps5sx2_id_and_no_rom_arguments(cli):
    out,req=fake_start(cli,'PS2MENU','/mnt/usb1/ps2/Example.iso')
    assert out.startswith('1|')
    assert len(req)==0x1020
    assert struct.unpack_from('<II',req,0)==(0x4C574650,1)
    assert req[0x08:0x18]==b'PPSA99203'+b'\0'*7
    assert struct.unpack_from('<II',req,0x18)==(0,0)
    assert req[0x20:]==b'\0'*4096

def test_ps2_response_status_kept_distinct(cli):
    good,_=fake_start(cli,'PS2MENU','unused',stage=0,rc=0)
    denied,_=fake_start(cli,'PS2MENU','unused',stage=3,rc=0x8094000F)
    assert good.startswith('0|')
    assert denied.startswith('3|')

def test_ps2_missing_forwarder_stays_no_service(cli):
    with socket.socket() as sock:
        sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    run=subprocess.run([str(cli),'PS2MENU','unused',str(port)],capture_output=True,text=True,timeout=5)
    assert run.returncode==0 and run.stdout.startswith('2|')

def test_other_four_platform_packets_not_regressed(cli):
    for platform,value,target,argc in [
        ('PS5','PPSA21567',b'PPSA21567',0),
        ('PS4','CUSA00031',b'CUSA00031',0),
        ('Switch','/mnt/usb1/switch/Test.nsp',b'PPSA99008',3),
        ('Xbox 360','/mnt/usb1/xbox360/Test.iso',b'PPSA50011',1),
    ]:
        _,req=fake_start(cli,platform,value)
        assert req[0x08:0x11]==target
        assert struct.unpack_from('<I',req,0x18)[0]==argc

def test_ps2_ui_is_menu_only_and_uses_same_bridge():
    gui=(CPP/'universal_launcher.cpp').read_text()
    client=(CPP/'launcher_client.cpp').read_text()
    header=(CPP/'launcher_client.hpp').read_text()
    block=gui.split('else if(g.platform=="PS2"){')[1].split('else if(!plan.supported)')[0]
    assert 'const auto ps2_plan=ulg::make_ps5sx2_menu_launch_plan();' in block
    assert 'ulg::send_launch(ps2_plan)' in block
    assert 'if(bridge.status==ulg::LaunchStatus::queued)sys::quit();' in block
    assert 'stage_ps5sx2_disc_launch(g.path)' in block
    assert 'launch_emulator_menu(' not in block
    assert 'ui::Button::cross,hint(menu_mode?"Menue":"Spielen")' in gui
    assert 'PS2-Spielpfad via disc-launch.txt; Quadrat: Menue' in gui
    assert 'p.target="PPSA99203";' in client
    assert 'make_ps5sx2_menu_launch_plan();' in header
    assert 'BGH V1.1.26' in gui

def test_launcher_tunnel_remains_passive_and_other_handlers_preserve_v127_launch_protocol():
    import hashlib
    # V1.0.5 intentionally stages recent history before handoff; hashes pin the new blocks.
    gui=(CPP/'universal_launcher.cpp').read_text()
    ps5=gui.split('else if(g.platform=="PS5"){')[1].split('else if(g.platform=="PS4"){')[0]
    ps4=gui.split('else if(g.platform=="PS4"){')[1].split('else if(g.platform=="PS2"){')[0]
    assert hashlib.sha256(ps5.encode()).hexdigest()=="6ac541fa6547caa4d1d0359f90b67fabf8a87ab521acea5c738b592468030a11"
    assert hashlib.sha256(ps4.encode()).hexdigest()=="98fbaa1a84e3a5b1bfb72eb768ebde1a6cddee78fd9a34b7c854c492e49a4102"
    client=(CPP/'launcher_client.cpp').read_text()
    assert 'p.target="PPSA99008";p.argv={"--rom",game_path,"--exit-after-game"}' in client
    assert 'p.target="PPSA50011";p.argv={game_path}' in client
