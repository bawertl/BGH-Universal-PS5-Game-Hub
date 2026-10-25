"""V0.12.14: blocking connect without fcntl; no network payload/launch request."""
from pathlib import Path
import socket
import subprocess
import threading
import pytest

ROOT=Path(__file__).resolve().parents[1]

@pytest.fixture(scope="module")
def probe_bin(tmp_path_factory):
    p=tmp_path_factory.mktemp("blocking_probe")
    (p/"check.cpp").write_text(r'''
#include "launcher_client.hpp"
#include <cstdio>
#include <cstdlib>
int main(int argc,char**argv){
 if(argc!=2)return 2;
 auto x=ulg::probe_loopback_tcp(static_cast<unsigned short>(std::atoi(argv[1])),300);
 std::printf("%d|%d|%s\n",x.reachable?1:0,x.os_error,ulg::probe_stage_name(x.stage));
}
''')
    out=p/"check"
    subprocess.run(["g++","-std=c++20","-Wall","-Wextra","-Werror","-I",str(ROOT/"native_ps5"),
         str(p/"check.cpp"),str(ROOT/"native_ps5/launcher_client.cpp"),"-o",str(out)],
         check=True,capture_output=True,text=True)
    return out

def test_probe_uses_same_blocking_connect_approach_as_existing_clients():
    code=(ROOT/"native_ps5/launcher_client.cpp").read_text()
    probe=code.split("LocalPortProbe probe_loopback_tcp(")[1].split("LocalLaunchServices probe_local_launch_services()")[0]
    assert "fcntl(fd" not in probe
    assert "select(" not in probe
    assert "connect(fd," in probe
    assert "setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO" in probe
    assert "setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO" in probe
    assert "result.stage=PortProbeStage::connect_call;" in probe
    for forbidden in ("send(","write(","psfwd_ensure_launcher(","system(","fork(","launcher.elf"):
        assert forbidden not in probe

def test_open_listener_is_tcp_only(probe_bin):
    with socket.socket() as server:
        server.bind(("127.0.0.1",0));server.listen(1);server.settimeout(3)
        port=server.getsockname()[1];received=[]
        def serve():
            conn,_=server.accept()
            with conn:
                conn.settimeout(2)
                received.append(conn.recv(32))
        th=threading.Thread(target=serve,daemon=True);th.start()
        result=subprocess.run([str(probe_bin),str(port)],capture_output=True,text=True,timeout=4)
        th.join(timeout=3)
        assert result.returncode==0,result.stderr
        assert result.stdout.strip()=="1|0|OK"
        assert received==[b""]

def test_closed_port_returns_connect_stage(probe_bin):
    with socket.socket() as server:
        server.bind(("127.0.0.1",0));port=server.getsockname()[1]
    result=subprocess.run([str(probe_bin),str(port)],capture_output=True,text=True,timeout=4)
    assert result.returncode==0,result.stderr
    x=result.stdout.strip().split("|")
    assert x[0]=="0" and int(x[1])!=0 and x[2]=="CONNECT"

def test_launch_and_cover_code_protected():
    code=(ROOT/"native_ps5/universal_launcher.cpp").read_text()
    assert "BGH V1.1.27" in code
    assert "const auto bridge=ulg::send_launch(ps5_plan);" in code
    assert "if(bridge.status==ulg::LaunchStatus::queued)sys::quit();" in code
    assert 'format_port("2121 FTP",local_services_.ftp)' in code
