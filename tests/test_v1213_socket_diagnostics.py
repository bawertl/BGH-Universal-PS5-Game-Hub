"""BGH V0.12.14: preserve the V0.12.12 behavior; show socket cause and FTP comparison."""
from pathlib import Path
import socket
import subprocess
import threading

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def diag_cli(tmp_path_factory):
    temp = tmp_path_factory.mktemp("v1213_probe")
    main = temp / "main.cpp"
    main.write_text(r'''
#include "launcher_client.hpp"
#include <cstdio>
#include <cstdlib>
int main(int argc,char**argv){
 if(argc!=2)return 3;
 unsigned long port=std::strtoul(argv[1],nullptr,10);
 if(port>65535)return 4;
 auto p=ulg::probe_loopback_tcp(static_cast<unsigned short>(port),300);
 std::printf("%d|%d|%s|%s\n",p.reachable?1:0,p.os_error,
  ulg::probe_errno_name(p.os_error),ulg::probe_stage_name(p.stage));
 return 0;
}''')
    exe = temp / "port-diagnostic"
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "native_ps5"),
                    str(main), str(ROOT / "native_ps5/launcher_client.cpp"), "-o", str(exe)],
                   capture_output=True, text=True, check=True)
    return exe


def test_open_listener_does_not_receive_payload(diag_cli):
    with socket.socket() as srv:
        srv.bind(("127.0.0.1",0))
        srv.listen(1)
        srv.settimeout(2)
        payloads=[]
        def runner():
            client,_=srv.accept()
            with client:
                client.settimeout(2)
                payloads.append(client.recv(16))
        th=threading.Thread(target=runner,daemon=True)
        th.start()
        run=subprocess.run([str(diag_cli),str(srv.getsockname()[1])],capture_output=True,text=True,timeout=4)
        th.join(timeout=3)
        assert run.returncode==0,run.stderr
        assert run.stdout.strip()=="1|0|OK|OK"
        assert payloads==[b""],"Probe must not write any bytes"


def test_refused_listener_returns_named_os_error(diag_cli):
    with socket.socket() as sock:
        sock.bind(("127.0.0.1",0))
        unused_port=sock.getsockname()[1]
    run=subprocess.run([str(diag_cli),str(unused_port)],capture_output=True,text=True,timeout=4)
    assert run.returncode==0,run.stderr
    data=run.stdout.strip().split("|")
    assert data[0]=="0"
    assert int(data[1])!=0
    assert data[2]=="ECONNREFUSED"  # Linux host; PS5 errno may differ
    assert data[3]=="CONNECT"


def test_invalid_port_stage_is_visible(diag_cli):
    run=subprocess.run([str(diag_cli),"0"],capture_output=True,text=True,timeout=4)
    assert run.returncode==0
    assert run.stdout.strip().split("|")[2:]==["EINVAL","PARAM"]


def test_only_manual_noninvasive_loopback_diagnostic():
    client=(ROOT/"native_ps5/launcher_client.cpp").read_text()
    gui=(ROOT/"native_ps5/universal_launcher.cpp").read_text()
    header=(ROOT/"native_ps5/launcher_client.hpp").read_text()
    workflow=(ROOT/".github/workflows/ps5-build.yml").read_text()
    assert "probes.launcher=probe_loopback_tcp(10199);" in client
    assert "probes.elfldr=probe_loopback_tcp(9021);" in client
    assert "probes.ftp=probe_loopback_tcp(2121);" in client
    probe=client.split("LocalPortProbe probe_loopback_tcp(")[1].split("LocalLaunchServices probe_local_launch_services()")[0]
    for dangerous in ("send(","write(","sceSystemService","launcher.elf","exec", "system(","fork("):
        assert dangerous not in probe
    assert "addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);" in probe
    assert "fcntl(fd" not in probe
    assert "SO_SNDTIMEO" in probe
    assert "SO_RCVTIMEO" in probe
    assert "connect(fd," in probe
    assert "result.stage=PortProbeStage::connect_call;" in probe
    assert 'ulg::probe_errno_name(p.os_error)' in gui
    assert 'ulg::probe_stage_name(p.stage)' in gui
    assert 'format_port("2121 FTP",local_services_.ftp)' in gui
    assert 'local_services_=ulg::probe_local_launch_services();' in gui
    assert 'if(input.is_pressed(Action::west) && !indices_.empty())' in gui
    assert 'BGH V1.1.31' in gui
    assert "bool local_services_checked_=false" in gui
    assert 'PortProbeStage stage' in header
    assert 'bgh-v1.1.31' in workflow
    assert 'V1_1_31_PPSA99079_PS5_TEST.zip' in workflow


def test_all_original_ps5_launch_paths_remain():
    gui=(ROOT/"native_ps5/universal_launcher.cpp").read_text()
    assert 'const auto bridge=ulg::send_launch(ps5_plan);' in gui
    assert 'const auto native=ulg::launch_registered_ps5(g.title_id);' in gui
    assert 'if(bridge.status==ulg::LaunchStatus::queued)sys::quit();' in gui
    assert "make_registered_ps5_launch_plan(g.title_id)" in gui
