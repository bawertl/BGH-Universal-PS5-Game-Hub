"""V0.12.9: deterministic host regressions for platform filtering and duplicates."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / 'native_ps5'


def compile_harness(tmp_path: Path, code: str):
    source = tmp_path / 'test.cpp'
    source.write_text(code)
    exe = tmp_path / 'test'
    subprocess.run(['g++', '-std=c++20', '-O0', '-Wall', '-Wextra', '-Werror',
                    '-I', str(CPP), str(source), str(CPP / 'library_core.cpp'),
                    '-o', str(exe)], check=True)
    return exe


def test_off_means_off_duplicate_ps5_games_emulators_and_apps(tmp_path):
    cover = tmp_path / 'existing.png'
    cover.write_bytes(b'png-test-cover-do-not-decode')
    code = r'''
#include "library_core.hpp"
#include <cassert>
#include <string>
using namespace ulg;
static Game game(const std::string &id,const std::string &name,const std::string &platform,
                 const std::string &path,const std::string &tid,bool available=true){
    Game g;g.id=id;g.title=name;g.platform=platform;g.path=path;
    g.title_id=tid;g.available=available;g.format="installationsordner";return g;
}
int main(int argc,char**argv){
    assert(argc==2);
    Catalogue c;
    c.games.push_back(game("a","Wolverine","PS5","/on/PPSA03671","PPSA03671"));
    c.games.push_back(game("b","Wolverine","PS5","/on/Wolverine-PPSA03671","PPSA03671"));
    c.games.back().cover_path=argv[1];
    c.games.push_back(game("c","Wolverine","PS5","/off/PPSA03671","PPSA03671",false));
    c.games.push_back(game("d","App Dumper","PS5","/on/APDU00001",""));
    c.games.push_back(game("e","BlackBox","PS5","/on/BLKB00001",""));
    c.games.push_back(game("f","Emulator","Homebrew","/on/PPSA99203","PPSA99203"));
    c.games.push_back(game("g","Random utility","Homebrew","/on/OTHER0001",""));
    c.games.push_back(game("h","Installed","PS4","/ps4/CUSA00031","CUSA00031"));
    c.games.push_back(game("i","Installed","PS4","/ps4/CUSA00031-alt","CUSA00031"));
    c.games.push_back(game("j","Switch title","Switch","/roms/game1.nsp",""));
    c.games.push_back(game("k","Switch title","Switch","/roms/game2.nsp",""));
    c.games[9].format="nsp";c.games[10].format="nsp";
    std::vector<Root> roots={{"PS5","/on",true},{"PS5","/off",false},
                             {"PS4","/ps4",true},{"Switch","/roms",true}};
    clean_catalogue(c,roots);
    int ps5=0,ps4=0,emu=0,sw=0;
    for(const auto &g:c.games){
        if(g.platform=="PS5") {++ps5;assert(g.title=="Wolverine");assert(g.cover_path==argv[1]);}
        if(g.platform=="PS4") ++ps4;
        if(g.platform=="Homebrew") {++emu;assert(g.title_id=="PPSA99203");}
        if(g.platform=="Switch")++sw;
    }
    assert(c.games.size()==4 && ps5==1 && ps4==1 && emu==1 && sw==1);
    Catalogue old,now;
    old.games.push_back(game("m","Trine","PS5","/off/PPSA14275","PPSA14275"));
    now.roots={{"PS5","/off","deaktiviert",0,""}};
    auto merged=merge_unavailable(now,old);
    assert(merged.games.empty()); // OFF must not resurrect old entries.
    Catalogue all_off;
    all_off.games.push_back(game("z","Offline","PS5","/off/PPSA03671","PPSA03671"));
    clean_catalogue(all_off,{{"PS5","/off",false}});
    assert(all_off.games.empty());
}
'''
    exe = compile_harness(tmp_path, code)
    subprocess.run([str(exe), str(cover)], check=True)


def test_embedded_ppsa_and_ps4_package_scanner_are_still_correct(tmp_path):
    ps5 = tmp_path / 'PS5'
    ps5.mkdir()
    g5 = ps5 / 'Pistol.Whip-PPSA12514-EUR-Game'
    (g5 / 'sce_sys').mkdir(parents=True)
    (g5 / 'eboot.bin').write_bytes(b'')
    # The folder name alone carries a valid ID; no fake title metadata required.
    ps4 = tmp_path / 'PS4'
    ps4.mkdir()
    folder = ps4 / 'CUSA00031'
    folder.mkdir()
    (folder / 'app.pkg').write_bytes(b'package')
    paths = tmp_path / 'paths.conf'
    paths.write_text(f'PS5|{ps5}\nPS4|{ps4}\n')
    output = tmp_path / 'games.json'
    subprocess.run([str(CPP / 'scanner-host'), '--paths', str(paths), '--output', str(output)], check=True)
    import json
    games = json.loads(output.read_text())['games']
    assert sorted((g['title_id'], g['platform']) for g in games) == [
        ('CUSA00031','PS4'),('PPSA12514','PS5')]


def test_gui_and_workflow_contain_verified_v129_mechanics():
    ui = (CPP / 'universal_launcher.cpp').read_text()
    lib = (CPP / 'library_core.cpp').read_text()
    ftp = (CPP / 'ftp_scan.cpp').read_text()
    workflow = (ROOT / '.github/workflows/ps5-build.yml').read_text()
    assert 'library_v113_ps5_cover.bin' in ui and 'library_v129.bin untouched' in ui and 'ulg::clean_catalogue(catalogue_,roots_)' in ui
    assert 'settings_visible_roots()' in ui
    assert 'root.status == "ftp_ok" || root.status == "deaktiviert"' in lib
    assert 'game.platform=="PS5" && valid_native_id(game.title_id,"PPSA")' in ftp
    assert 'bgh-v1.1.31-fix1' in workflow
    assert '|| true' not in workflow


def test_ps5_cover_recovery_from_appmeta_over_readonly_ftp(tmp_path):
    """An inaccessible PS5 appmeta icon is imported to private cache via FTP."""
    import base64
    import socket
    import socketserver
    import threading
    png = base64.b64decode(
        'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/yEsAAAAASUVORK5CYII=')
    source = tmp_path / 'recover.cpp'
    source.write_text(r'''
#include "ftp_scan.hpp"
#include <cassert>
#include <cstdlib>
#include <sys/stat.h>
int main(int argc,char**argv){
 if(argc!=3)return 10;
 std::vector<ulg::Root> r={{"PS5","/mock_ps5"}};
 auto c=ulg::scan(r);
 ulg::supplement_from_local_ftp(c,r,4,50,std::atoi(argv[1]),argv[2]);
 assert(c.games.size()==1);
 const auto &g=c.games[0];
 assert(g.platform=="PS5" && g.title_id=="PPSA14275");
 assert(g.cover_path.find(argv[2])==0);
 struct stat s{};assert(stat(g.cover_path.c_str(),&s)==0 && s.st_size>20);
}
''')
    exe = tmp_path / 'recover'
    subprocess.run(['g++', '-std=c++20', '-O0', '-I', str(CPP), str(source),
                    str(CPP/'ftp_scan.cpp'), str(CPP/'ftp_readonly.cpp'),
                    str(CPP/'library_core.cpp'), '-o', str(exe)], check=True)
    commands = []

    class Handler(socketserver.StreamRequestHandler):
        def handle(self):
            cwd = ''
            pasv = None
            def reply(m): self.wfile.write((m+'\r\n').encode('ascii'))
            reply('220 Test FTP')
            try:
                while command := self.rfile.readline():
                    line = command.decode('ascii', 'replace').strip()
                    commands.append(line)
                    if line.startswith('USER '): reply('230 OK')
                    elif line == 'TYPE I': reply('200 OK')
                    elif line.startswith('CWD '):
                        cwd = line[4:];reply('250 OK')
                    elif line == 'PASV':
                        pasv = socket.socket();pasv.bind(('127.0.0.1',0));pasv.listen(1)
                        pasv.settimeout(4)
                        port=pasv.getsockname()[1]
                        reply(f'227 Passive (127,0,0,1,{port//256},{port%256})')
                    elif line == 'MLSD':
                        if cwd == '/mock_ps5': payload=b'type=dir; PPSA14275\r\n'
                        elif cwd == '/mock_ps5/PPSA14275': payload=b'type=dir; sce_sys\r\n'
                        else: payload=b''
                        reply('150 Transfer')
                        with pasv.accept()[0] as data: data.sendall(payload)
                        pasv.close();pasv=None;reply('226 Done')
                    elif line.startswith('RETR '):
                        path=line[5:]
                        content = {
                            '/mock_ps5/PPSA14275/sce_sys/param.json': b'{"titleId":"PPSA14275","titleName":"Trine 5"}',
                            '/user/appmeta/PPSA14275/icon0.png': png,
                        }.get(path)
                        if content is None:
                            reply('550 Missing')
                            if pasv:pasv.close();pasv=None
                        else:
                            reply('150 Transfer')
                            with pasv.accept()[0] as data:data.sendall(content)
                            pasv.close();pasv=None;reply('226 Done')
                    else:reply('502 unsupported')
            finally:
                if pasv:pasv.close()

    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True
    srv=Server(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=srv.serve_forever,daemon=True)
    thread.start()
    cache=tmp_path/'covers';cache.mkdir()
    try:
        result=subprocess.run([str(exe),str(srv.server_address[1]),str(cache)],
                              text=True,capture_output=True,timeout=25)
    finally:
        srv.shutdown();srv.server_close();thread.join(5)
    assert result.returncode==0, (result.stdout,result.stderr,commands)
    assert 'RETR /user/appmeta/PPSA14275/icon0.png' in commands
    assert not any(cmd.startswith(('STOR ','DELE ','MKD ','RMD ','SITE ')) for cmd in commands)
