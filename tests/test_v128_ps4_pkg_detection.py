"""BGH V0.12.8: PS4 installed PKG + separate appmeta cover, read-only."""
from pathlib import Path
import base64
import json
import socket
import socketserver
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / "native_ps5"
PNG = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/yEsAAAAASUVORK5CYII=")
KNOWN_CUSA = ["CUSA00031", "CUSA00234", "CUSA01126", "CUSA03082", "CUSA11898", "CUSA18824", "CUSA35474"]


def test_exactly_seven_real_installation_structures_and_existing_ps2_ps5(tmp_path):
    installed = tmp_path / "user" / "app"
    installed.mkdir(parents=True)
    for title_id in KNOWN_CUSA:
        folder = installed / title_id
        folder.mkdir()
        (folder / "app.pkg").write_bytes(b"installed-package-placeholder")
        (folder / "app.json").write_text('{"numberOfSplitFiles":1,"originalFileSize":12345}')
    invalid = installed / "CUSA12345_NOT_INSTALLED"  # No valid title ID folder.
    invalid.mkdir()
    (invalid / "app.pkg").write_bytes(b"PKG")
    (installed / "Random.pkg").write_bytes(b"download-only")
    (installed / "CUSA00032").mkdir()  # ID without package is not sufficient.
    ps5 = tmp_path / "homebrew" / "PPSA12345" / "sce_sys"
    ps5.mkdir(parents=True)
    (ps5 / "param.json").write_text('{"titleId":"PPSA12345","titleName":"PS5 Referenz"}')
    ps2 = tmp_path / "PS5SX2"
    ps2.mkdir()
    (ps2 / "Spiel.iso").write_bytes(b"PS2 data")
    config = tmp_path / "paths.conf"
    config.write_text(f"PS4|{installed}\nPS5|{ps5.parent.parent}\nPS2|{ps2}\n")
    result = tmp_path / "results.json"
    subprocess.run([str(NATIVE / "scanner-host"), "--paths", str(config), "--output", str(result)], check=True)
    games = json.loads(result.read_text())["games"]
    ps4_games = [g for g in games if g["platform"] == "PS4"]
    assert sorted(g["title_id"] for g in ps4_games) == KNOWN_CUSA
    assert all(g["format"] == "installationsordner" and g["path"].endswith(g["title_id"]) for g in ps4_games)
    assert len(games) == 9
    assert len([g for g in games if g["platform"] == "PS5"]) == 1
    assert len([g for g in games if g["platform"] == "PS2"]) == 1
    for g in ps4_games:
        assert "CUSA" in g["title"]  # No unverified name reconstructed from app.json.


def test_ps4_root_migration_preserves_saved_preferences_and_idempotence(tmp_path):
    source = tmp_path / "rootcheck.cpp"
    source.write_text(r'''
#include "root_manager.hpp"
#include <cassert>
int main(){
 std::vector<ulg::Root> r={{"PS4","/mnt/usb0",false},{"PS5","/data/homebrew",true},{"PS2","/my/ps2",true}};
 assert(ulg::ensure_ps4_install_root(r));
 assert(r.size()==4 && r[3].platform=="PS4" && r[3].path=="/user/app" && r[3].enabled);
 assert(!r[0].enabled && r[0].path=="/mnt/usb0" && r[2].path=="/my/ps2");
 assert(!ulg::ensure_ps4_install_root(r));
 r[3].enabled=false;
 assert(!ulg::ensure_ps4_install_root(r));
 assert(!r[3].enabled);  // Do not forcibly re-enable a manually disabled root.
}
''')
    exe = tmp_path / "rootcheck"
    subprocess.run(["g++", "-std=c++20", "-I", str(NATIVE), str(source),
                    str(NATIVE / "root_manager.cpp"), str(NATIVE / "library_core.cpp"),
                    str(NATIVE / "ftp_readonly.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    assert "PS4|/user/app" in (ROOT / "assets/paths.conf").read_text()


def test_ftp_recovers_ps4_install_and_matching_appmeta_png(tmp_path):
    source = tmp_path / "ps4_ftp.cpp"
    source.write_text(r'''
#include "ftp_scan.hpp"
#include <cassert>
#include <sys/stat.h>
#include <string>
int main(int argc,char** argv){
 if(argc!=3)return 9;
 std::vector<ulg::Root> roots{{"PS4","/user/app"}};
 auto c=ulg::scan(roots);
 ulg::supplement_from_local_ftp(c,roots,5,500,atoi(argv[1]),argv[2]);
 assert(c.games.size()==1);
 assert(c.games[0].platform=="PS4" && c.games[0].title_id=="CUSA00031");
 assert(c.games[0].path=="/user/app/CUSA00031");
 assert(c.games[0].format=="installationsordner" && c.games[0].via_ftp);
 struct stat st{};
 assert(!c.games[0].cover_path.empty() && stat(c.games[0].cover_path.c_str(),&st)==0 && st.st_size>20);
 return 0;
}
''')
    exe = tmp_path / "ps4_ftp"
    subprocess.run(["g++", "-std=c++20", "-I", str(NATIVE), str(source),
                    str(NATIVE / "library_core.cpp"), str(NATIVE / "ftp_scan.cpp"),
                    str(NATIVE / "ftp_readonly.cpp"), "-o", str(exe)], check=True)
    commands = []
    class Handler(socketserver.StreamRequestHandler):
        def handle(self):
            cwd = ""
            pasv = None
            def reply(msg):
                self.wfile.write((msg + "\r\n").encode())
            reply("220 local test FTP")
            try:
                while line := self.rfile.readline():
                    command = line.decode("ascii", "replace").strip()
                    commands.append(command)
                    if command.startswith("USER "):
                        reply("230 OK")
                    elif command == "TYPE I":
                        reply("200 OK")
                    elif command.startswith("CWD "):
                        cwd = command[4:]
                        reply("250 OK")
                    elif command == "PASV":
                        pasv = socket.socket()
                        pasv.bind(("127.0.0.1", 0))
                        pasv.listen(1)
                        pasv.settimeout(5)
                        port = pasv.getsockname()[1]
                        reply(f"227 Passive (127,0,0,1,{port // 256},{port % 256})")
                    elif command == "MLSD":
                        if cwd == "/user/app":
                            payload = b"type=dir; CUSA00031\r\n"
                        elif cwd == "/user/app/CUSA00031":
                            payload = b"type=file; app.pkg\r\ntype=file; app.json\r\n"
                        else:
                            payload = b""
                        reply("150 Transfer")
                        with pasv.accept()[0] as data:
                            data.sendall(payload)
                        pasv.close(); pasv = None
                        reply("226 Done")
                    elif command.startswith("RETR "):
                        if command[5:] == "/user/appmeta/CUSA00031/icon0.png":
                            reply("150 Transfer")
                            with pasv.accept()[0] as data:
                                data.sendall(PNG)
                            pasv.close(); pasv = None
                            reply("226 Done")
                        else:
                            reply("550 Missing")
                            if pasv: pasv.close(); pasv = None
                    else:
                        reply("502 unsupported")
            finally:
                if pasv: pasv.close()

    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True

    server = Server(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    cover_cache = tmp_path / "covers"
    cover_cache.mkdir()
    try:
        run = subprocess.run([str(exe), str(server.server_address[1]), str(cover_cache)],
                             text=True, capture_output=True, timeout=35)
    finally:
        server.shutdown(); server.server_close(); thread.join(4)
    assert run.returncode == 0, (run.stdout, run.stderr, commands)
    assert "RETR /user/appmeta/CUSA00031/icon0.png" in commands
    assert not any(c.startswith(("STOR ", "DELE ", "MKD ", "RMD ", "SITE ")) for c in commands)
    assert any(cover_cache.glob("*.png"))
