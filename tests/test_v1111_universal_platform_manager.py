"""BGH 1.1.1: platform registry v1 migration, multi-roots, uniform sorting."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
REG=(ROOT/'native_ps5/platform_registry.hpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()


def test_new_settings_navigation_and_sidebar_are_global():
    assert 'return ulg::platform_order(platform_registry_);' in CPP
    assert 'emulator_expanded_=!emulator_expanded_' in CPP
    assert 'emulator_subselected_' in CPP
    assert 'begin_emulator_path_editor(app,folder,true)' in CPP
    assert 'remove_emulator_folder(folder)' in CPP
    assert 'sidebar_profile_visible(p)' in CPP
    assert 'p.enabled;' in REG
    assert '"PS5","PlayStation 5"' in REG
    assert '"PS4","PlayStation 4"' in REG
    assert '"+ Weiteren Spieleordner hinzufuegen"' in CPP


def test_no_unsupported_backends_or_changed_launch_routes():
    for name in ('PS3','Dreamcast','GameCube','Wii','Xbox'):
        assert 'add("'+name+'"' in REG
    assert 'return "Vorlage / kein Backend"' in CPP
    assert 'make_registered_ps5_launch_plan(g.title_id)' in CPP
    assert 'make_registered_ps4_launch_plan(g.title_id)' in CPP
    assert 'stage_ps5sx2_disc_launch(g.path)' in CPP
    assert 'ulg::send_launch(emulator_menu_plan(' in CPP
    assert 'kQuietSeconds = 0.035f' in (ROOT/'native_ps5/cover_load_debounce.hpp').read_text()


def test_legacy_registry_migration_multi_path_roundtrip(tmp_path):
    source=tmp_path/'v1111_registry.cpp'
    source.write_text(r'''
#include "platform_registry.hpp"
#include <cassert>
#include <fstream>
#include <string>
#include <cstdio>
int main(int argc,char** argv){
    assert(argc==3);
    const std::string current=argv[1],old=argv[2];
    auto registry=ulg::default_platform_registry();
    assert(registry.entries[0].id=="PS5");
    assert(!ulg::sidebar_profile_visible(registry.entries[7]));
    registry.entries[7].enabled=true;
    assert(ulg::sidebar_profile_visible(registry.entries[7])); // PS3 visible without a scanner
    assert(ulg::move_platform(registry,"PS2",-1));
    assert(ulg::move_platform(registry,"PS2",-1));
    assert(registry.entries[ulg::platform_order(registry)[0]].id=="PS2");
    auto &p=registry.entries[2];
    assert(ulg::add_profile_folder(p,"/mnt/usb1/PS2"));
    assert(ulg::add_profile_folder(p,"/data/PCSX2"));
    assert(!ulg::add_profile_folder(p,"/data/PCSX2"));
    p.app_path="/data/homebrew/PPSA99203";
    registry.revision=23;
    assert(ulg::write_platform_registry(registry,current));
    ulg::PlatformRegistry loaded;
    assert(ulg::read_platform_registry(current,loaded));
    assert(loaded.revision==23 && loaded.entries[7].enabled);
    assert(loaded.entries[2].rom_folders.size()==2);
    assert(loaded.entries[2].rom_folders[0]=="/mnt/usb1/PS2");
    assert(loaded.entries[2].rom_folders[1]=="/data/PCSX2");
    assert(loaded.entries[2].app_path=="/data/homebrew/PPSA99203");
    assert(loaded.entries[ulg::platform_order(loaded)[0]].id=="PS2");
    assert(ulg::remove_profile_folder(loaded.entries[2],0));
    assert(loaded.entries[2].rom_folders.size()==1);
    assert(loaded.entries[2].rom_folder=="/data/PCSX2");
    assert(ulg::write_platform_registry(loaded,current));
    assert(ulg::read_platform_registry(current,registry));
    assert(registry.entries[2].rom_folders.size()==1);
    // Construct a real v1 registry (all 12 profiles, original checksum).
    auto v1=ulg::default_platform_registry();
    v1.entries[2].rom_folder="/mnt/usb1/Old";
    v1.entries[7].enabled=true;
    v1.revision=22;
    std::string body="ULGR|1|22\n";
    for(const auto &entry:v1.entries)
        body+=entry.id+"|"+(entry.enabled?"1":"0")+"|"+
            std::to_string(entry.sort_order)+"|"+entry.rom_folder+"\n";
    char crc[16];std::snprintf(crc,sizeof(crc),"%08X",ulg::platform_checksum(body));
    {std::ofstream f(old,std::ios::binary);f<<body<<"CRC|"<<crc<<"\n";}
    ulg::PlatformRegistry migrated;
    assert(ulg::read_platform_registry(old,migrated));
    assert(migrated.entries[2].rom_folders.size()==1);
    assert(migrated.entries[2].rom_folders[0]=="/mnt/usb1/Old");
    assert(migrated.entries[7].enabled);
    assert(ulg::write_platform_registry(migrated,current));
    assert(ulg::read_platform_registry(current,registry));
    assert(registry.entries[2].rom_folders.size()==1);
    // Corrupt backup cannot clobber a previously loaded registry.
    {std::fstream f(current,std::ios::in|std::ios::out|std::ios::binary);
        f.seekp(9);f.put('!');}
    assert(!ulg::read_platform_registry(current,registry));
    assert(registry.entries[2].rom_folders[0]=="/mnt/usb1/Old");
    assert(!ulg::safe_platform_path("/data/a;b"));
    assert(!ulg::safe_platform_path("/mnt/../etc"));
    assert(ulg::safe_platform_path("/mnt/usb1/PS2"));
}
''')
    binary=tmp_path/'v1111_registry'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
                    '-I',str(ROOT/'native_ps5'),str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(tmp_path/'new.bin'),str(tmp_path/'old.bin')],check=True)


def test_version_icons_and_source_are_packed():
    from PIL import Image
    for name in ('ps1','psp','ps3','xbox','dreamcast','gamecube','wii'):
        with Image.open(ROOT/'assets/platforms'/f'{name}.png') as image:
            assert image.mode=='RGBA' and image.size==(96,96)
            assert image.getchannel('A').getbbox() is not None
    assert 'branches: [bgh-v1.1.30]' in WF
    assert 'BGH-V1-1-30-PPSA99079' in WF
    assert 'test_v1111_universal_platform_manager.py' in WF
    assert 'platform_registry.hpp' in (ROOT/'tooling/install_ui_kit_overlay.py').read_text()
