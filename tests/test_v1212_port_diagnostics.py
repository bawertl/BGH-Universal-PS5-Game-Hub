"""BGH V0.12.12 local, non-invasive TCP reachability probe tests."""
from pathlib import Path
import socket
import subprocess
import threading

import pytest

ROOT = Path(__file__).resolve().parents[1]

@pytest.fixture(scope="module")
def probe_cli(tmp_path_factory):
    folder = tmp_path_factory.mktemp("port_probe")
    main = folder / "probe.cpp"
    main.write_text(r'''
#include "launcher_client.hpp"
#include <cstdio>
#include <cstdlib>
int main(int argc, char** argv){
    if(argc!=2)return 3;
    const auto port=static_cast<unsigned short>(std::strtoul(argv[1],nullptr,10));
    const auto result=ulg::probe_loopback_tcp(port,300);
    std::printf("%d|%d\n",result.reachable?1:0,result.os_error);
    return 0;
}
''')
    exe = folder / "probe"
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "native_ps5"),
                    str(main), str(ROOT / "native_ps5/launcher_client.cpp"), "-o", str(exe)], check=True,
                   capture_output=True, text=True)
    return exe


def test_open_local_listener_detected_without_sending_payload(probe_cli):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.1", 0))
        server.listen(1)
        server.settimeout(3)
        port = server.getsockname()[1]
        got = []

        def accept_probe():
            sock, _ = server.accept()
            with sock:
                sock.settimeout(2)
                got.append(sock.recv(4096))
        thread = threading.Thread(target=accept_probe, daemon=True)
        thread.start()
        result = subprocess.run([str(probe_cli), str(port)], text=True, capture_output=True, timeout=5)
        thread.join(timeout=3)
        assert result.returncode == 0, result.stderr
        assert result.stdout.startswith("1|0"), result.stdout
        assert got == [b""], "Portprobe darf keinerlei Daten senden"


def test_closed_port_is_not_reported_open(probe_cli):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.bind(("127.0.0.1", 0))
        port = server.getsockname()[1]
    result = subprocess.run([str(probe_cli), str(port)], text=True, capture_output=True, timeout=5)
    assert result.returncode == 0
    assert result.stdout.startswith("0|"), result.stdout


def test_probe_only_on_square_in_native_game_detail():
    gui = (ROOT / "native_ps5/universal_launcher.cpp").read_text()
    client = (ROOT / "native_ps5/launcher_client.cpp").read_text()
    assert 'if(input.is_pressed(Action::west) && !indices_.empty())' in gui
    assert 'if(g.platform=="PS5" || g.platform=="PS4"){' in gui
    assert 'local_services_=ulg::probe_local_launch_services();' in gui
    assert 'local_services_checked_=false' in gui
    assert 'Quadrat: TCP 10199 / 9021 / 2121 (Diagnose)' in gui
    assert 'probes.launcher=probe_loopback_tcp(10199);' in client
    assert 'probes.elfldr=probe_loopback_tcp(9021);' in client
    probe = client.split('LocalPortProbe probe_loopback_tcp(')[1].split('LocalLaunchServices probe_local_launch_services()')[0]
    for forbidden in ['send(', 'write(', 'sceSystemService', 'launcher.elf', 'exec', 'system(', 'fork(']:
        assert forbidden not in probe


def test_preserve_unmodified_scanners_covers_launch_path_and_version():
    gui = (ROOT / "native_ps5/universal_launcher.cpp").read_text()
    assert 'BGH V1.1.27' in gui
    assert 'const auto bridge=ulg::send_launch(ps5_plan);' in gui
    assert 'const auto native=ulg::launch_registered_ps5(g.title_id);' in gui
    assert 'if(bridge.status==ulg::LaunchStatus::queued)sys::quit();' in gui
    workflow = (ROOT / ".github/workflows/ps5-build.yml").read_text()
    assert 'bgh-v1.1.27' in workflow
    assert 'V1_1_27_PPSA99079_PS5_TEST.zip' in workflow
