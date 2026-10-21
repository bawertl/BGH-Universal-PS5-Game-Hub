"""PS5 USB container art: read-only FTP lookup by PPSA ID (host simulation)."""
import socket
import socketserver
import subprocess
import threading
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / "native_ps5"
CONTAINERS = {
    "PPSA06084_Superliminal.ffpfsc": "PPSA06084",
    "PPSA08804 - Sonic Racing.ffpfsc": "PPSA08804",
    "PPSA24864 -The Midnight.exfat": "PPSA24864",
    "PPSA28997 - God of War son of sparta.exfat": "PPSA28997",
    "REANIMAL-PPSA23372.ffpfsc": "PPSA23372",
}


def png(pixel):
    def chunk(name, payload):
        return struct.pack(">I", len(payload)) + name + payload + struct.pack(">I", zlib.crc32(name + payload) & 0xffffffff)
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(b"\x00" + bytes(pixel))) +
            chunk(b"IEND", b""))


def test_usb_container_title_id_maps_existing_appmeta_png_without_touching_image(tmp_path):
    root = tmp_path / "usb1"
    root.mkdir()
    source_contents = {}
    for filename in CONTAINERS:
        payload = b"independent-container-contents:" + filename.encode()
        source_contents[filename] = payload
        (root / filename).write_bytes(payload)
    # This game intentionally has no metadata and must not inherit another image.
    missing = "PPSA99999 - Not Installed.exfat"
    (root / missing).write_bytes(b"missing-art-data")

    expected = {}
    for i, (filename, title_id) in enumerate(CONTAINERS.items()):
        expected[f"/user/appmeta/{title_id}/icon0.png"] = png((10+i*25, 20+i*20, 30+i*10, 255))

    cache = tmp_path / "covers"
    cache.mkdir()
    cpp = tmp_path / "verify.cpp"
    cpp.write_text(r'''
#include "library_core.hpp"
#include "ftp_scan.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
int main(int argc,char **argv){
    assert(argc==4);
    std::vector<ulg::Root> roots={{"PS5",argv[1]}};
    auto lib=ulg::scan(roots,2,50);
    assert(lib.games.size()==6 && lib.roots[0].status=="ok");
    ulg::supplement_from_local_ftp(lib,roots,2,50,std::atoi(argv[2]),argv[3]);
    for(const auto &g:lib.games){
        assert(g.platform=="PS5");
        assert(g.format=="exfat" || g.format=="ffpfsc");
        std::cout << g.title_id << '\t' << g.cover_path << '\n';
    }
}
''')
    exe = tmp_path / "test-binary"
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-O0",
                    "-I", str(NATIVE), str(cpp),
                    *[str(NATIVE / s) for s in ("library_core.cpp", "ftp_scan.cpp", "ftp_readonly.cpp")],
                    "-o", str(exe)], check=True, timeout=70)

    calls = []

    class Handler(socketserver.StreamRequestHandler):
        def handle(self):
            passive = None
            self.wfile.write(b"220 Server\r\n")

            def answer(text):
                self.wfile.write((text + "\r\n").encode())

            try:
                while line := self.rfile.readline():
                    cmd = line.decode("utf-8", "replace").strip()
                    calls.append(cmd)
                    if cmd.startswith("USER "):
                        answer("230 Logged in")
                    elif cmd == "TYPE I":
                        answer("200 Binary")
                    elif cmd == "PASV":
                        passive = socket.socket()
                        passive.bind(("127.0.0.1", 0))
                        passive.listen(1)
                        passive.settimeout(4)
                        port = passive.getsockname()[1]
                        answer(f"227 Passive (127,0,0,1,{port // 256},{port % 256})")
                    elif cmd.startswith("RETR "):
                        requested = cmd[5:]
                        if requested not in expected:
                            answer("550 Missing")
                            if passive:
                                passive.close()
                                passive = None
                        else:
                            answer("150 Transfer")
                            with passive.accept()[0] as data:
                                data.sendall(expected[requested])
                            passive.close()
                            passive = None
                            answer("226 Done")
                    else:
                        answer("502 Unsupported")
            finally:
                if passive:
                    passive.close()

    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True

    server = Server(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        result = subprocess.run([str(exe), str(root), str(server.server_address[1]), str(cache)],
                                capture_output=True, text=True, timeout=40)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=3)
    assert result.returncode == 0, (result.stdout, result.stderr, calls)
    records = dict(line.split("\t", 1) for line in result.stdout.splitlines())
    for file, title_id in CONTAINERS.items():
        assert records[title_id], (title_id, calls)
        assert Path(records[title_id]).read_bytes() == expected[f"/user/appmeta/{title_id}/icon0.png"]
        assert (root / file).read_bytes() == source_contents[file]
        assert calls.count(f"RETR /user/appmeta/{title_id}/icon0.png") == 1
    assert records["PPSA99999"] == ""
    assert not any(cmd.startswith(("STOR ", "DELE ", "MKD ", "RMD ", "SITE ")) for cmd in calls)
    assert not any(".ffpfsc" in cmd or ".exfat" in cmd for cmd in calls if cmd.startswith("RETR "))


def test_release_does_not_change_launchers_or_image_decoder():
    code = (NATIVE / "universal_launcher.cpp").read_text()
    ftp = (NATIVE / "ftp_scan.cpp").read_text()
    assert 'stage_ps5sx2_disc_launch(g.path)' in code
    assert 'ulg::send_launch(ps5_plan)' in code
    assert 'library_v116_container_start_title.bin' in code
    assert 'container_cover_budget=128' in ftp
    assert '"/user/appmeta/"+game.title_id+"/icon0.png"' in ftp
    assert 'const bool container=(game.platform=="PS5"' in ftp
