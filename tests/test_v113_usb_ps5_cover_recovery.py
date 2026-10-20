"""BGH V1.1.3 PS5 USB: recover cover of a POSIX-scanned app via FTP."""
import base64
import socket
import socketserver
import subprocess
import threading
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / "native_ps5"
PNG = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/yEsAAAAASUVORK5CYII="
)


def test_native_ps5_usb_local_scan_recovers_game_folder_icon_without_appmeta(tmp_path):
    """Healthy scan root must not prevent read-only FTP per-game artwork recovery."""
    root = tmp_path / "homebrew"
    game = root / "PPSA25642-app0"
    (game / "sce_sys").mkdir(parents=True)
    (game / "eboot.bin").write_bytes(b"placeholder")
    (game / "sce_sys" / "param.json").write_text(
        '{"titleId":"PPSA25642","titleName":"Clive Barker\'s Hellraiser: Revival"}'
    )
    source = tmp_path / "test.cpp"
    source.write_text(r'''
#include "ftp_scan.hpp"
#include <cassert>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
int main(int argc,char**argv){
    assert(argc==4);
    std::vector<ulg::Root> roots={{"PS5",argv[1]}};
    auto c=ulg::scan(roots);
    assert(c.games.size()==1);
    assert(c.roots.size()==1 && c.roots[0].status=="ok");
    assert(c.games[0].platform=="PS5" && c.games[0].title_id=="PPSA25642");
    assert(c.games[0].cover_path.empty());
    ulg::supplement_from_local_ftp(c,roots,4,50,std::atoi(argv[2]),argv[3]);
    assert(c.games.size()==1);
    assert(c.games[0].path==std::string(argv[1])+"/PPSA25642-app0");
    const auto &cover=c.games[0].cover_path;
    assert(!cover.empty() && cover.find(argv[3])==0);
    struct stat st{};assert(stat(cover.c_str(),&st)==0 && st.st_size>20);
}
''')
    exe = tmp_path / "verify"
    subprocess.run(["g++", "-std=c++20", "-O0", "-Wall", "-Wextra", "-Werror",
                    "-I", str(NATIVE), str(source),
                    str(NATIVE / "library_core.cpp"),
                    str(NATIVE / "ftp_scan.cpp"),
                    str(NATIVE / "ftp_readonly.cpp"), "-o", str(exe)],
                   check=True, timeout=35)
    commands = []
    target = f"{game}/sce_sys/icon0.png"

    class Handler(socketserver.StreamRequestHandler):
        def handle(self):
            pasv = None
            def reply(message):
                self.wfile.write((message + "\r\n").encode("ascii"))
            reply("220 Ready")
            try:
                while line := self.rfile.readline():
                    cmd = line.decode("ascii", "replace").strip()
                    commands.append(cmd)
                    if cmd.startswith("USER "):
                        reply("230 OK")
                    elif cmd == "TYPE I":
                        reply("200 OK")
                    elif cmd == "PASV":
                        pasv = socket.socket()
                        pasv.bind(("127.0.0.1", 0))
                        pasv.listen(1)
                        pasv.settimeout(5)
                        port = pasv.getsockname()[1]
                        reply(f"227 Passive (127,0,0,1,{port // 256},{port % 256})")
                    elif cmd.startswith("RETR "):
                        if cmd[5:] != target:
                            reply("550 Missing")
                            if pasv:
                                pasv.close()
                                pasv = None
                        else:
                            reply("150 Ready")
                            with pasv.accept()[0] as stream:
                                stream.sendall(PNG)
                            pasv.close()
                            pasv = None
                            reply("226 Complete")
                    else:
                        reply("502 Unsupported")
            finally:
                if pasv:
                    pasv.close()

    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True

    server = Server(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    cache = tmp_path / "cache"
    cache.mkdir()
    try:
        result = subprocess.run([str(exe), str(root), str(server.server_address[1]), str(cache)],
                                capture_output=True, text=True, timeout=25)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(5)
    assert result.returncode == 0, (result.stdout, result.stderr, commands)
    assert f"RETR {target}" in commands
    assert not any(c.startswith(("STOR ", "DELE ", "MKD ", "RMD ", "SITE ")) for c in commands)
    assert not list((game / "sce_sys").glob("*.png"))  # game folder remains untouched


def test_new_cache_rebuild_and_unmodified_launch_interface():
    ui = (NATIVE / "universal_launcher.cpp").read_text()
    ftp = (NATIVE / "ftp_scan.cpp").read_text()
    assert 'library_v113_ps5_cover.bin' in ui
    assert 'ulg::send_launch(ps5_plan)' in ui
    assert 'game.path+"/"+name' in ftp
    assert '"sce_sys/icon0.png"' in ftp
    assert 'if(game.platform=="PS5")' in ftp
    assert 'const bool installed=game.format=="installationsordner";' in ftp
