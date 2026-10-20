"""PS5 USB root containers and FTP artwork budget regression (host only)."""
import base64
import socket
import socketserver
import subprocess
import threading
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / "native_ps5"
PNG = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/yEsAAAAASUVORK5CYII=")
FILES = [
    "PPSA06084_Superliminal.ffpfsc",
    "PPSA08804 - Sonic Racing.ffpfsc",
    "PPSA24864 -The Midnight.exfat",
    "PPSA28997 - God of War son of sparta.exfat",
    "REANIMAL-PPSA23372.ffpfsc",
]


def compile_test(tmp_path, source, *sources):
    code = tmp_path / "host_test.cpp"
    code.write_text(source)
    binary = tmp_path / "verify"
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-O0",
                    "-I", str(NATIVE), str(code),
                    *[str(NATIVE / s) for s in sources], "-o", str(binary)],
                   check=True, timeout=60)
    return binary


def test_posix_ps5_container_file_scan_and_no_ps4_false_positive(tmp_path):
    usb = tmp_path / "usb1"
    usb.mkdir()
    for filename in FILES:
        (usb / filename).write_bytes(b"testonly")
    (usb / "unrelated.exe").write_bytes(b"not a PS5 game")
    (usb / "PPSA67890_Unrelated.zip").write_bytes(b"not a PS5 container")
    installed = usb / "PPSA49999-app0" / "sce_sys"
    installed.mkdir(parents=True)
    (installed / "param.json").write_text('{"titleId":"PPSA49999","titleName":"Installed PS5"}')
    binary = compile_test(tmp_path, r'''
#include "library_core.hpp"
#include <cassert>
#include <set>
#include <string>
int main(int argc,char**argv){
    assert(argc==2);
    std::vector<ulg::Root> roots={{"PS5",argv[1]}};
    auto result=ulg::scan(roots,3,100);
    assert(result.games.size()==6);
    std::set<std::string> formats;
    std::set<std::string> ids;
    for(const auto &g:result.games){
        assert(g.platform=="PS5" && g.available);
        formats.insert(g.format);
        ids.insert(g.title_id);
    }
    assert(formats.count("ffpfsc") && formats.count("exfat") && formats.count("installationsordner"));
    assert(ids.count("PPSA23372") && ids.count("PPSA06084") && ids.count("PPSA49999"));
    ulg::clean_catalogue(result,roots);
    assert(result.games.size()==6);
    auto ps4=ulg::scan({{"PS4",argv[1]}},3,100);
    assert(ps4.games.empty());
}''', "library_core.cpp")
    subprocess.run([binary, usb], check=True, timeout=30)


def test_legacy_root_extension_respects_explicit_disable(tmp_path):
    binary = compile_test(tmp_path, r'''
#include "root_manager.hpp"
#include <cassert>
int main(){
    std::vector<ulg::Root> roots={{"PS5","/mnt/usb1/homebrew"},{"PS4","/user/app"}};
    assert(ulg::ensure_ps5_usb_parent_root(roots));
    assert(roots.size()==3 && roots.back().path=="/mnt/usb1" && roots.back().enabled);
    assert(!ulg::ensure_ps5_usb_parent_root(roots));
    std::vector<ulg::Root> disabled={{"PS5","/mnt/usb1/homebrew"},{"PS5","/mnt/usb1",false}};
    assert(!ulg::ensure_ps5_usb_parent_root(disabled));
    assert(!disabled.back().enabled);
    std::vector<ulg::Root> unrelated={{"PS5","/mnt/usb0/homebrew"}};
    assert(!ulg::ensure_ps5_usb_parent_root(unrelated));
}''', "root_manager.cpp", "library_core.cpp", "ftp_readonly.cpp")
    subprocess.run([binary], check=True, timeout=30)


class FtpMock(socketserver.StreamRequestHandler):
    def handle(self):
        passive = None
        root = self.server.scan_root
        self.wfile.write(b"220 Ready\r\n")
        def reply(txt):
            self.wfile.write((txt + "\r\n").encode())
        try:
            while line := self.rfile.readline():
                cmd = line.decode("utf-8", "replace").strip()
                self.server.commands.append(cmd)
                if cmd.startswith("USER "):
                    reply("230 Authenticated")
                elif cmd == "TYPE I":
                    reply("200 Binary")
                elif cmd.startswith("CWD "):
                    reply("250 OK" if cmd[4:] == str(root) else "550 Missing")
                elif cmd == "PASV":
                    passive = socket.socket()
                    passive.bind(("127.0.0.1", 0))
                    passive.listen(1)
                    passive.settimeout(5)
                    port = passive.getsockname()[1]
                    reply(f"227 Passive (127,0,0,1,{port // 256},{port % 256})")
                elif cmd == "MLSD":
                    reply("150 Data")
                    with passive.accept()[0] as connection:
                        connection.sendall(b"".join(f"type=file;size=100; {name}\r\n".encode() for name in FILES))
                    passive.close()
                    passive = None
                    reply("226 Done")
                elif cmd.startswith("RETR "):
                    path = cmd[5:]
                    if path.endswith("/sce_sys/icon0.png") and path.startswith(str(root) + "/"):
                        reply("150 Data")
                        with passive.accept()[0] as connection:
                            connection.sendall(PNG)
                        passive.close()
                        passive = None
                        reply("226 Done")
                    else:
                        reply("550 Missing")
                        if passive:
                            passive.close()
                            passive = None
                else:
                    reply("502 Unsupported")
        finally:
            if passive:
                passive.close()


class FtpServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def run_ftp(root, binary, args):
    server = FtpServer(("127.0.0.1", 0), FtpMock)
    server.scan_root = root
    server.commands = []
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        result = subprocess.run([str(binary), str(server.server_address[1]), *map(str, args)],
                                capture_output=True, text=True, timeout=50)
        assert result.returncode == 0, (result.stdout, result.stderr, server.commands)
        return server.commands
    finally:
        server.shutdown()
        server.server_close()
        thread.join(3)


def test_ftp_only_finds_five_container_files_without_reading_them(tmp_path):
    unmounted = tmp_path / "unmounted"
    binary = compile_test(tmp_path, r'''
#include "ftp_scan.hpp"
#include <cassert>
#include <cstdlib>
#include <set>
int main(int argc,char**argv){
    assert(argc==3);
    std::vector<ulg::Root> roots={{"PS5",argv[2]}};
    auto result=ulg::scan(roots,2,100);
    assert(result.games.empty() && result.roots[0].status!="ok");
    ulg::supplement_from_local_ftp(result,roots,2,100,std::atoi(argv[1]));
    assert(result.games.size()==5);
    std::set<std::string> formats;
    for(const auto &g:result.games){assert(g.platform=="PS5" && g.via_ftp);formats.insert(g.format);}
    assert(formats.count("ffpfsc") && formats.count("exfat"));
}''', "library_core.cpp", "ftp_scan.cpp", "ftp_readonly.cpp")
    commands = run_ftp(unmounted, binary, [unmounted])
    assert any(c == "MLSD" for c in commands)
    assert not any(c.startswith(("RETR ", "STOR ", "DELE ")) for c in commands)


def test_17_existing_usb_icons_do_not_starve_later_titles(tmp_path):
    root = tmp_path / "homebrew"
    for number in range(17):
        game = root / f"PPSA{30000 + number}-app0" / "sce_sys"
        game.mkdir(parents=True)
        (game / "param.json").write_text(
            '{"titleId":"PPSA' + str(30000 + number) + '","titleName":"Synthetic"}')
    cache = tmp_path / "cache"
    cache.mkdir()
    binary = compile_test(tmp_path, r'''
#include "ftp_scan.hpp"
#include <cassert>
#include <cstdlib>
int main(int argc,char**argv){
    assert(argc==4);
    std::vector<ulg::Root> roots={{"PS5",argv[2]}};
    auto result=ulg::scan(roots,3,100);
    assert(result.games.size()==17);
    ulg::supplement_from_local_ftp(result,roots,3,100,std::atoi(argv[1]),argv[3]);
    for(const auto &g:result.games)assert(!g.cover_path.empty());
    assert(result.failed_operations==0);
}''', "library_core.cpp", "ftp_scan.cpp", "ftp_readonly.cpp")
    commands = run_ftp(root, binary, [root, cache])
    last = f"PPSA30016-app0/sce_sys/icon0.png"
    assert any(c.startswith("RETR ") and c.endswith(last) for c in commands)
    assert sum(c.startswith("RETR ") for c in commands) == 17
    assert not any(c.startswith(("STOR ", "DELE ")) for c in commands)


def test_no_changes_to_registered_launch_and_fresh_private_catalogue():
    ui = (NATIVE / "universal_launcher.cpp").read_text()
    assert 'ulg::send_launch(ps5_plan)' in ui
    assert 'stage_ps5sx2_disc_launch(g.path)' in ui
    assert 'library_v115_container_cover.bin' in ui
    assert 'PS5-Container erkannt; erst separat mounten/registrieren' in ui
    assert 'missing_usb_ps5_cover' in ui
