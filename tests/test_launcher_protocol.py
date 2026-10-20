"""Fake loopback launcher tests. Never sends to a real PS5 or real launcher."""
import socket
import struct
import subprocess
import threading
from pathlib import Path
import pytest


@pytest.fixture(scope='session')
def client():
    root=Path(__file__).resolve().parents[1]
    subprocess.run(['make','-C',str(root/'native_ps5'),'CXX=g++','launch-test-cli'],check=True,capture_output=True)
    return root/'native_ps5'/'launch-test-cli'


def invoke_mock(client,platform,path,stage,rc=0):
    with socket.socket() as server:
        server.bind(('127.0.0.1',0));server.listen(1);port=server.getsockname()[1]
        captured=[]
        def reply():
            peer,_=server.accept()
            with peer:
                data=b''
                while len(data)<0x1020:
                    read=peer.recv(0x1020-len(data))
                    if not read:break
                    data+=read
                captured.append(data)
                payload=struct.pack('<6I',0x4C574650,1,stage,rc,rc,1234)
                peer.sendall(payload)
        t=threading.Thread(target=reply,daemon=True);t.start()
        result=subprocess.run([str(client),platform,path,str(port)],capture_output=True,text=True,timeout=4)
        t.join(timeout=2)
        assert result.returncode==0,result.stderr
        return result.stdout,captured


def test_switch_direct_launch_protocol(client):
    result,captured=invoke_mock(client,'Switch','/mnt/usb1/eden/roms/Zelda.xci',3,0x80940010)
    packet=captured[0]
    assert len(packet)==0x1020
    magic,version=struct.unpack_from('<II',packet,0)
    argc,length=struct.unpack_from('<II',packet,0x18)
    assert (magic,version)==(0x4C574650,1)
    assert packet[8:17]==b'PPSA99008'
    assert argc==3
    assert packet[0x20:0x20+length]==b'--rom\0/mnt/usb1/eden/roms/Zelda.xci\0--exit-after-game\0'
    assert result.startswith('1|')


def test_xbox_direct_launch_protocol(client):
    result,captured=invoke_mock(client,'Xbox 360','/mnt/usb1/xbox360/Halo/default.xex',0)
    packet=captured[0]
    assert packet[8:17]==b'PPSA50011'
    argc,length=struct.unpack_from('<II',packet,0x18)
    assert argc==1
    assert packet[0x20:0x20+length]==b'/mnt/usb1/xbox360/Halo/default.xex\0'
    assert result.startswith('0|')


def test_unsupported_platform_never_connects(client):
    out=subprocess.run([str(client),'PS2','/mnt/usb1/PS5SX2/games/FF.iso','19992'],capture_output=True,text=True,timeout=2)
    assert out.stdout.startswith('4|')


def test_absent_launcher_is_explicitly_reported(client):
    with socket.socket() as server:
        server.bind(('127.0.0.1',0));port=server.getsockname()[1]
    out=subprocess.run([str(client),'Switch','/data/prosperoeden/roms/a.nsp',str(port)],capture_output=True,text=True,timeout=2)
    assert out.stdout.startswith('2|')
