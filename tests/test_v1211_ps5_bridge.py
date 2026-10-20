"""PS5 Forwarder-Format client tests. Local mock only; NO real-console access."""
import socket
import struct
import subprocess
import threading
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def cli():
    subprocess.run(["make", "-B", "-C", str(ROOT / "native_ps5"), "CXX=g++", "launch-test-cli"],
                   check=True, capture_output=True, text=True)
    return ROOT / "native_ps5" / "launch-test-cli"


def run_mock(cli, title, stage=3, rc1=0x80940010, rc2=0x80940010):
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        sock.listen(1)
        sock.settimeout(3)
        port = sock.getsockname()[1]
        packets = []

        def handle():
            peer, _ = sock.accept()
            with peer:
                data = b""
                while len(data) < 0x1020:
                    chunk = peer.recv(0x1020 - len(data))
                    if not chunk:
                        break
                    data += chunk
                packets.append(data)
                peer.sendall(struct.pack("<6I", 0x4C574650, 1, stage, rc1, rc2, 7))

        thread = threading.Thread(target=handle, daemon=True)
        thread.start()
        proc = subprocess.run([str(cli), "PS5", title, str(port)],
                              capture_output=True, text=True, timeout=5)
        thread.join(timeout=3)
        assert proc.returncode == 0, proc.stderr
        return proc.stdout, packets


def test_ps5_registered_title_only_no_args(cli):
    out, packets = run_mock(cli, "PPSA14275")
    assert out.startswith("1|"), out  # daemon queued the launch
    assert len(packets) == 1
    p = packets[0]
    assert len(p) == 0x1020
    assert struct.unpack_from("<II", p, 0) == (0x4C574650, 1)
    assert p[8:24] == b"PPSA14275" + b"\0" * 7
    assert struct.unpack_from("<II", p, 0x18) == (0, 0)
    assert p[0x20:] == b"\0" * 4096


def test_daemon_started_stage_is_not_refused(cli):
    out, packets = run_mock(cli, "PPSA14275", stage=0, rc1=0, rc2=0)
    assert out.startswith("0|"), out
    assert len(packets) == 1


def test_daemon_refusal_never_counts_as_accepted(cli):
    out, packets = run_mock(cli, "PPSA14275", stage=3, rc1=0x8094000F, rc2=0x8094000F)
    assert out.startswith("3|"), out
    assert len(packets) == 1


@pytest.mark.parametrize("id", ["CUSA00031", "PPSA99079", "PPSA99008", "PPSA99203",
                                   "PPSA50011", "PPSA1234", "PPSA12A45", "../PPSA14275"])
def test_invalid_or_protected_titles_do_not_contact_service(cli, id):
    # Deliberately do not have a listener: the client must reject locally.
    out = subprocess.run([str(cli), "PS5", id, "19993"],
                         capture_output=True, text=True, timeout=3)
    assert out.returncode == 0
    assert out.stdout.startswith("4|"), out.stdout


def test_no_resident_daemon_falls_back_safely(cli):
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    out = subprocess.run([str(cli), "PS5", "PPSA14275", str(port)],
                         capture_output=True, text=True, timeout=4)
    assert out.returncode == 0
    assert out.stdout.startswith("2|"), out.stdout


def test_ui_uses_service_then_original_native_fallback():
    source = (ROOT / "native_ps5/universal_launcher.cpp").read_text()
    assert "make_registered_ps5_launch_plan(g.title_id)" in source
    assert "const auto bridge=ulg::send_launch(ps5_plan);" in source
    assert "else if(bridge.status==ulg::LaunchStatus::no_launcher)" in source
    assert "const auto native=ulg::launch_registered_ps5(g.title_id);" in source
    assert "if(bridge.status==ulg::LaunchStatus::queued)sys::quit();" in source
    assert "PPSA99079" not in (ROOT / "native_ps5/launcher_client.cpp").read_text().split("p.target=title_id;")[1][:200]
