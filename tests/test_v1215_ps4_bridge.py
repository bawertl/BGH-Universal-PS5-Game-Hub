"""V0.12.16: PS4/CUSA start bridge is additive to tested PS5 behavior.
Host mock tests never contact a PS5 or start payloads.
"""
import socket
import struct
import subprocess
import threading
from pathlib import Path
import pytest

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / 'native_ps5'

@pytest.fixture(scope='module')
def launch_cli():
    subprocess.run(['make', '-B', '-C', str(CPP), 'CXX=g++', 'launch-test-cli'],
                   check=True, capture_output=True, text=True)
    return CPP / 'launch-test-cli'


def _fake_daemon(launch_cli, platform, title_id, stage=3, rc1=0x80940010, rc2=0x80940010):
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0)); sock.listen(1); sock.settimeout(3)
        port = sock.getsockname()[1]
        packets = []
        def handle():
            peer, _ = sock.accept()
            with peer:
                data = b''
                while len(data) < 0x1020:
                    incoming = peer.recv(0x1020-len(data))
                    if not incoming: break
                    data += incoming
                packets.append(data)
                peer.sendall(struct.pack('<6I', 0x4C574650, 1, stage, rc1, rc2, 7))
        th = threading.Thread(target=handle, daemon=True); th.start()
        run = subprocess.run([str(launch_cli), platform, title_id, str(port)],
                             capture_output=True, text=True, timeout=5)
        th.join(timeout=3)
        assert run.returncode == 0, run.stderr
        return run.stdout, packets


def test_cusa_request_is_protocol_compatible_and_no_arguments(launch_cli):
    out, packets = _fake_daemon(launch_cli, 'PS4', 'CUSA00031')
    assert out.startswith('1|')  # queued
    assert len(packets) == 1 and len(packets[0]) == 0x1020
    payload = packets[0]
    assert struct.unpack_from('<II', payload, 0) == (0x4C574650, 1)
    assert payload[0x08:0x18] == b'CUSA00031' + b'\0'*7
    assert struct.unpack_from('<II', payload, 0x18) == (0, 0)
    assert payload[0x20:] == b'\0' * 4096


def test_ps4_started_and_refused_are_distinct(launch_cli):
    started, _ = _fake_daemon(launch_cli, 'PS4', 'CUSA00031', stage=0, rc1=0, rc2=0)
    denied, _ = _fake_daemon(launch_cli, 'PS4', 'CUSA00031', stage=3,
                             rc1=0x8094000F, rc2=0x8094000F)
    assert started.startswith('0|')
    assert denied.startswith('3|')


@pytest.mark.parametrize('title_id', [
    'PPSA21567', 'CUSA', 'CUSA0003', 'CUSA00031X', 'CUSA00X31', 'cusa00031',
    'CUSA1234 ', 'CUSA/0001', 'PPSA99079'
])
def test_invalid_cusa_does_not_contact_service(launch_cli, title_id):
    p = subprocess.run([str(launch_cli), 'PS4', title_id, '19993'],
                       capture_output=True, text=True, timeout=4)
    assert p.returncode == 0
    assert p.stdout.startswith('4|')


def test_ps4_missing_forwarder_never_executes_native_fallback(launch_cli):
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0)); port = sock.getsockname()[1]
    p = subprocess.run([str(launch_cli), 'PS4', 'CUSA00031', str(port)],
                       capture_output=True, text=True, timeout=4)
    assert p.returncode == 0 and p.stdout.startswith('2|')
    gui = (CPP / 'universal_launcher.cpp').read_text()
    ps4_block = gui.split('else if(g.platform=="PS4"){')[1].split('else if(!plan.supported)')[0]
    assert 'launch_registered_ps5' not in ps4_block
    assert 'make_registered_ps4_launch_plan(g.title_id)' in ps4_block
    assert 'const auto bridge=ulg::send_launch(ps4_plan);' in ps4_block
    assert 'if(bridge.status==ulg::LaunchStatus::queued)sys::quit();' in ps4_block
    assert 'rc1=0x%08X rc2=0x%08X' in ps4_block
    assert 'g.platform=="PS5"' in gui and 'make_registered_ps5_launch_plan(g.title_id)' in gui


def test_clean_ps4_catalogue_fills_and_normalizes_title_ids(tmp_path):
    harness = tmp_path / 'ps4catalogue.cpp'
    harness.write_text(r'''
#include "library_core.hpp"
#include <cassert>
using namespace ulg;
static Game game(const char *id,const char *title,const char *path,const char *tid,const char *platform="PS4"){
    Game g;g.id=id;g.title=title;g.path=path;g.platform=platform;
    g.title_id=tid;g.available=true;g.format="installationsordner";return g;
}
int main(){
    Catalogue c;
    c.games.push_back(game("a","PS4 missing","/ps4/CUSA00031",""));
    c.games.push_back(game("b","PS4 lowercase","/ps4/another-CUSA00031","cusa00031"));
    c.games.push_back(game("c","PS4 bad meta","/ps4/CUSA00032","??garbage"));
    c.games.push_back(game("d","PS4 invalid","/ps4/no_cusa_here",""));
    c.games.push_back(game("e","PS5 original","/ps5/PPSA14275","PPSA14275","PS5"));
    std::vector<Root> roots={{"PS4","/ps4",true},{"PS5","/ps5",true}};
    clean_catalogue(c,roots);
    int four=0,five=0;
    for(const auto &g:c.games){
        if(g.platform=="PS4"){
            ++four;
            assert(g.title_id=="CUSA00031" || g.title_id=="CUSA00032");
        }
        if(g.platform=="PS5"){
            ++five;assert(g.title_id=="PPSA14275");
        }
    }
    assert(four==2 && five==1 && c.games.size()==3);
}
''')
    exe = tmp_path / 'ps4catalogue'
    subprocess.run(['g++', '-std=c++20', '-O0', '-Wall', '-Wextra', '-Werror', '-I', str(CPP),
                    str(harness), str(CPP / 'library_core.cpp'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


def test_ps4_title_visible_and_manual_diagnostic_preserved():
    gui = (CPP / 'universal_launcher.cpp').read_text()
    assert '"Title-ID: "+(g.title_id.empty()?' in gui
    assert 'if(g.platform=="PS5" || g.platform=="PS4"){' in gui
    assert 'ulg::probe_local_launch_services();' in gui
    assert 'g.platform=="PS4" && ulg::make_registered_ps4_launch_plan(g.title_id).supported' in gui
    assert 'BGH V1.1.27' in gui
