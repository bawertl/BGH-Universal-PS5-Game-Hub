"""BGH V1.1.9: automatic verified emulator tiles and PS3 disc folder art via FTP."""
from pathlib import Path
import io
import socket
import socketserver
import subprocess
import threading
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native_ps5'


def test_verified_emulator_auto_cards_are_view_only_and_deduplicated(tmp_path):
    cpp = tmp_path / 'test.cpp'
    exe = tmp_path / 'test'
    cpp.write_text(r'''
#include "library_core.hpp"
#include "emulator_cards.hpp"
#include <cassert>
int main(){
    auto reg=ulg::default_platform_registry();
    for(auto &p:reg.entries)if(p.id=="PS3"){
        p.enabled=true;p.app_path="/data/homebrew/PPSA99303";
    }
    std::map<std::string,ulg::emulator_detection::Detection> detections;
    ulg::emulator_detection::Detection known;
    known.title_id="PPSA99303";known.name="KB-RPCS3";known.via_ftp=true;
    known.status=ulg::emulator_detection::Detection::Status::known_emulator;
    detections["PS3"]=known;
    ulg::Catalogue cat;
    ulg::Game original;original.id="war-in-the-north";original.platform="PS3";
    original.path="/data/homebrew/PPSA99303/rpcs3/games/BLES01181";
    cat.games.push_back(original);
    ulg::emulator_cards::sync(cat,reg,detections);
    assert(cat.games.size()==2);
    assert(cat.games[0].id=="war-in-the-north");
    assert(cat.games[1].platform=="Homebrew");
    assert(cat.games[1].title_id=="PPSA99303");
    assert(cat.games[1].title=="KB-RPCS3");
    assert(cat.games[1].path=="/data/homebrew/PPSA99303");
    assert(cat.games[1].via_ftp);
    assert(ulg::emulator_cards::is_auto_card(cat.games[1]));
    // Re-rendering/refreshing must not generate duplicate emulator tiles.
    ulg::emulator_cards::sync(cat,reg,detections);
    assert(cat.games.size()==2);
    // Existing installed Homebrew game card should be preferred.
    auto installed=cat.games[1];installed.format="installationsordner";
    cat.games.push_back(installed);
    ulg::emulator_cards::sync(cat,reg,detections);
    assert(cat.games.size()==2);
    assert(cat.games[1].format=="installationsordner");
    // Removing a verified backend removes its auto tile, but not user ROMs.
    cat.games.pop_back();
    detections.clear();
    ulg::emulator_cards::sync(cat,reg,detections);
    assert(cat.games.size()==1);
    // A configured app with valid metadata is listed, clearly marked as lacking
    // a launch profile; actual startup remains forbidden in the dispatcher.
    known.title_id="PPSA90123";known.name="Future Emulator";
    known.status=ulg::emulator_detection::Detection::Status::unidentified_app;
    detections["PS3"]=known;
    ulg::emulator_cards::sync(cat,reg,detections);
    assert(cat.games.size()==2);
    assert(cat.games[1].title=="Future Emulator (Profil fehlt)");
    // Spoofed mismatch must never be auto-inserted.
    detections["PS3"].status=ulg::emulator_detection::Detection::Status::mismatch;
    ulg::emulator_cards::sync(cat,reg,detections);
    assert(cat.games.size()==1);
}
''')
    subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                    '-I', str(NATIVE), str(cpp), '-o', str(exe)],
                   check=True, capture_output=True)
    subprocess.run([str(exe)], check=True, timeout=10)


def test_gui_auto_emulator_tab_and_existing_rom_dispatch_preserved():
    code=(NATIVE/'universal_launcher.cpp').read_text()
    assert 'Emulatoren' in code and 'emulator_platform_for_title' in code
    assert 'ulg::emulator_cards::sync(catalogue_,platform_registry_,detected_emulators_)' in code
    assert 'detected_emulators_' in code
    assert 'const auto result=ulg::send_launch(menu_plan);' in code
    assert 'emulator_platform_for_title(selected_game.title_id)' in code
    assert 'ulg::active_emulator_title_id(p)' in code
    assert 'configured_rom_launch(const ulg::Game &game)' in code
    assert 'g.platform=="PS3"' in code
    assert 'kEmulatorCoverFolder' in code


def test_ps3_cover_imports_only_exact_game_icon_via_read_only_ftp(tmp_path):
    game_path = '/data/homebrew/PPSA99303/rpcs3/games/BLES01181 - War in the North'
    icon_path = game_path + '/PS3_GAME/ICON0.PNG'
    image = Image.new('RGBA', (320, 176), (100, 50, 175, 255))
    buffer = io.BytesIO(); image.save(buffer, 'PNG'); png=buffer.getvalue()
    commands=[]
    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            self.request.settimeout(7)
            reader=self.request.makefile('rb')
            listen=None
            self.request.sendall(b'220 BGH art test\r\n')
            try:
                while cmd:=reader.readline():
                    cmd=cmd.decode().strip();commands.append(cmd)
                    op=cmd.split(' ',1)[0]
                    if op=='USER': self.request.sendall(b'230 ok\r\n')
                    elif op=='TYPE': self.request.sendall(b'200 ok\r\n')
                    elif op=='CWD': self.request.sendall(b'250 ok\r\n' if cmd[4:]==game_path+'/PS3_GAME' else b'550 no\r\n')
                    elif op=='PASV':
                        listen=socket.socket();listen.bind(('127.0.0.1',0));listen.listen(1)
                        port=listen.getsockname()[1]
                        self.request.sendall(f'227 Entering Passive Mode (127,0,0,1,{port//256},{port%256})\r\n'.encode())
                    elif op in ('MLSD','RETR'):
                        if op=='RETR' and cmd[5:]!=icon_path:
                            self.request.sendall(b'550 no\r\n');listen.close();listen=None;continue
                        self.request.sendall(b'150 go\r\n')
                        conn,_=listen.accept()
                        with conn:conn.sendall(b'type=file; ICON0.PNG\r\n' if op=='MLSD' else png)
                        listen.close();listen=None
                        self.request.sendall(b'226 done\r\n')
                    else:self.request.sendall(b'500 no mutation\r\n')
            finally:
                reader.close()
                if listen:listen.close()
    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address=True;daemon_threads=True
    with Server(('127.0.0.1',0),Handler) as server:
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        try:
            cmd=[str(NATIVE/'emulator-cover-test-cli'),'import','PS3',game_path,'ps3-card-001',
                 'BLES01181',str(tmp_path/'none'),str(tmp_path),str(server.server_address[1]),
                 '', 'ps3_ordner']
            result=subprocess.run(cmd,text=True,capture_output=True,timeout=20)
            assert result.returncode==0, (result.stdout,result.stderr)
            assert 'FOUND|' in result.stdout
            assert 'ICON0.PNG' in result.stdout
            assert (tmp_path/'ps3-card-001.png').read_bytes()==png
        finally:
            server.shutdown();thread.join(timeout=5)
    assert 'RETR '+icon_path in commands
    assert not any(c.split(' ')[0] in {'STOR','DELE','RMD','MKD','SITE'} for c in commands)


def test_real_ps5_overlay_copies_the_new_emulator_cards_header():
    overlay=(ROOT/'tooling/install_ui_kit_overlay.py').read_text()
    assert "project/'native_ps5'/'emulator_cards.hpp'" in overlay
    assert "concepts/'emulator_cards.hpp'" in overlay
