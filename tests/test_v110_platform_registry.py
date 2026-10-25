"""V1.1.0 checksummed platform profiles, sidebar and four settings tabs."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
REG=(ROOT/'native_ps5/platform_registry.hpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()


def test_four_tabs_dynamic_sidebar_and_preserved_launch_paths():
    assert 'constexpr std::array<const char *,3> kSettingsTabs' in CPP
    assert 'draw_settings_emulators(list)' in CPP
    assert 'rebuild_sidebar()' in CPP
    assert 'platform_registry_.entries' in CPP
    assert 'sidebar_items_.size()' in CPP
    assert 'first+visible<n' in CPP
    assert 'ulg::send_launch(emulator_menu_plan(' in CPP
    assert 'stage_ps5sx2_disc_launch(g.path)' in CPP
    assert 'make_registered_ps5_launch_plan(g.title_id)' in CPP
    assert 'make_registered_ps4_launch_plan(g.title_id)' in CPP


def test_disabled_new_emulators_do_not_claim_support():
    assert 'add("PS3"' in REG and 'true,true,false,false' in REG
    assert 'add("PS1"' in REG and 'true,true,false,false' in REG
    assert 'add("Xbox"' in REG and 'true,true,false,false' in REG
    assert '"Vorlage / kein Backend"' in CPP or '"Vorlage / kein Backend"' in (ROOT/"native_ps5/emulator_detection.hpp").read_text()
    assert '"Scanner ja / Start offen"' in CPP


def test_platform_settings_are_isolated_and_atomic(tmp_path):
    test_file=tmp_path/'registry.cpp'
    test_file.write_text(r'''
#include "platform_registry.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
int main(int argc,char**argv){
    assert(argc==2);
    const std::string path=argv[1];
    auto original=ulg::default_platform_registry();
    assert(original.entries.size()>=12);
    assert(original.entries[0].id=="PS5" && original.entries[0].enabled);
    assert(original.entries[2].id=="PS2" && original.entries[2].enabled);
    assert(!original.entries[7].enabled);
    assert(!ulg::sidebar_profile_visible(original.entries[7]));
    assert(ulg::move_platform(original,"PS2",-1));
    assert(ulg::move_platform(original,"PS2",-1));
    const auto ordered=ulg::platform_order(original);
    assert(original.entries[ordered.front()].id=="PS2");
    original.entries[2].rom_folder="/mnt/usb1/PS2";
    original.revision=9;
    assert(ulg::write_platform_registry(original,path));
    ulg::PlatformRegistry fresh;
    assert(ulg::read_platform_registry(path,fresh));
    assert(fresh.revision==9 && fresh.entries[2].rom_folder=="/mnt/usb1/PS2");
    assert(fresh.entries[ulg::platform_order(fresh)[0]].id=="PS2");
    // Corruption cannot overwrite the caller's previously valid state.
    std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary);
    file.seekp(20);file.put('!');file.close();
    assert(!ulg::read_platform_registry(path,fresh));
    assert(fresh.revision==9);
    assert(!ulg::safe_platform_path("/mnt/../etc"));
    assert(!ulg::safe_platform_path("/a|b"));
    assert(ulg::safe_platform_path("/mnt/usb1/Switch"));
}
''')
    binary=tmp_path/'registry'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
                    '-I',str(ROOT/'native_ps5'),str(test_file),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(tmp_path/'platforms.bin')],check=True)


def test_workflow_version_and_overlay():
    assert 'branches: [bgh-v1.1.30-fix1]' in WF
    assert 'BGH-V1-1-30-FIX1-PPSA99079' in WF
    assert "platform_registry.hpp'" in (ROOT/'tooling/install_ui_kit_overlay.py').read_text()


def test_preset_icon_assets():
    from PIL import Image
    for icon in ('ps1','psp','ps3','xbox','dreamcast','gamecube','wii'):
        with Image.open(ROOT/'assets'/'platforms'/(icon+'.png')) as picture:
            assert picture.mode=='RGBA' and picture.size==(96,96)
