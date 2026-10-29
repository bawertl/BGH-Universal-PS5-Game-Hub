"""V1.1.20 PS2 one-shot reuse, RPCS3 app identity, and theme icons."""
from pathlib import Path
from PIL import Image
import subprocess

ROOT=Path(__file__).resolve().parents[1]
NATIVE=ROOT/'native_ps5'
UI=(NATIVE/'universal_launcher.cpp').read_text()

def test_rpcs3_is_a_homebrew_app_and_keeps_real_cover(tmp_path):
    art=tmp_path/'rpcs3.png'
    art.write_bytes(b'preexisting-art')
    src=tmp_path/'check.cpp'; exe=tmp_path/'check'
    src.write_text(r'''
#include "library_core.hpp"
#include "emulator_cards.hpp"
#include <cassert>
#include <map>
int main(int argc,char **argv){
 assert(argc==2);
 const std::string id="PPSA99303";
 assert(ulg::is_emulator_title_id(id));
 assert(ulg::classify_native_game("/data/homebrew/PPSA99303",{"RPCS3-5",id},true,false)=="Homebrew");
 assert(ulg::classify_native_game("/data/homebrew/PPSA12345",{"Game","PPSA12345"},true,false)=="PS5");
 ulg::Catalogue cat;
 ulg::Game existing;
 existing.id="rpcs3-app";existing.title="RPCS3-5";existing.platform="PS5";
 existing.path="/data/homebrew/PPSA99303";existing.title_id=id;
 existing.cover_path=argv[1];existing.format="installationsordner";
 cat.games.push_back(existing);
 ulg::clean_catalogue(cat,{{"PS5","/data/homebrew",true}});
 assert(cat.games.size()==1 && cat.games[0].platform=="Homebrew");
 assert(cat.games[0].cover_path==argv[1]);
 auto reg=ulg::default_platform_registry();
 for(auto &p:reg.entries)if(p.id=="PS3"){
   p.enabled=true;p.app_path="/data/homebrew/PPSA99303";
 }
 std::map<std::string,ulg::emulator_detection::Detection> detections;
 ulg::emulator_detection::Detection d;
 d.title_id=id;d.name="KB-RPCS3";
 d.status=ulg::emulator_detection::Detection::Status::known_emulator;
 detections["PS3"]=d;
 ulg::emulator_cards::sync(cat,reg,detections);
 assert(cat.games.size()==1);
 assert(cat.games[0].platform=="Homebrew" && cat.games[0].cover_path==argv[1]);
 // Old cached PS5 entries are corrected before synthesis/deduplication.
 cat.games[0].platform="PS5";
 ulg::emulator_cards::sync(cat,reg,detections);
 assert(cat.games.size()==1 && cat.games[0].platform=="Homebrew");
 assert(cat.games[0].cover_path==argv[1]);
}
''')
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-I',str(NATIVE),
                    str(src),str(NATIVE/'library_core.cpp'),'-o',str(exe)],
                   check=True,capture_output=True,text=True)
    subprocess.run([str(exe),str(art)],check=True,timeout=10)


def test_new_storage_art_is_crisp_transparent_and_loaded_not_only_declared():
    for name in ('storage_internal.png','storage_usb.png'):
        file=ROOT/'assets/themes'/name
        assert file.is_file()
        with Image.open(file) as image:
            assert image.size==(256,256) and image.mode=='RGBA'
            assert image.getchannel('A').getextrema()==(0,255)
        assert ('/app0/assets/themes/'+name) in UI
    storage=UI.split('void draw_storage_widgets(',1)[1].split('void draw_detail(',1)[0]
    assert 'if(tex)' in storage and 'list.image(tex,' in storage
    assert 'data.available' in storage and 'data.total' in storage
    assert 'glass_card(list,panel,15)' in storage


def test_controller_ten_pulses_over_two_seconds_without_pad_api_changes():
    draw=UI.split('void draw_controllers(',1)[1].split('// Hardware silhouettes',1)[0]
    assert 'elapsed<2.0f' in draw and 'elapsed/0.1f' in draw
    assert 'pad_active.png' in draw and 'pad_inactive.png' in draw
    assert 'scePadOpen(' not in draw


def test_ps2_ftp_uses_exact_single_filename_and_keeps_verification():
    src=(NATIVE/'ftp_ps2_writer.cpp').read_text()
    assert 'constexpr const char* kDirectory="/data/PCSX2/logs";' in src
    assert 'constexpr const char* kFile="disc-launch.txt";' in src
    assert 'result.message="disc-launch.txt existiert im FTP-Zielordner; nicht ueberschrieben"' not in src
    assert 'ftp.command(std::string("DELE ")+kFile)' in src
    assert 'if(!shared_request_equals(payload,port))' in src
    assert 'return result;' in src
    assert 'kAbsolute="/data/PCSX2/logs/disc-launch.txt"' in src


def test_release_identity_and_preserved_network_backends():
    wf=(ROOT/'.github/workflows/ps5-build.yml').read_text()
    assert 'bgh-v1.1.32' in wf
    assert 'BGH V1.1.32' in UI
    assert 'BGH V1.1.32' in UI
    assert 'PPSA99303' in UI
    assert 'ulg::shadow_storage::fetch_storage()' in UI
    for name in ('launcher_client.cpp','ftp_scan.cpp','ps5_launch.cpp', 'controller_status.hpp'):
        assert (NATIVE/name).is_file()
