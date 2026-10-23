"""BGH V1.1.9: verified PS3 --boot argument profile and safe generic per-emulator argv profiles."""
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native_ps5'


def test_ps3_known_rom_argv_and_unknown_emulator_denial(tmp_path):
    source = tmp_path / 'profile_probe.cpp'
    binary = tmp_path / 'profile_probe'
    source.write_text(r'''
#include "emulator_detection.hpp"
#include <cassert>
#include <string>
#include <vector>
int main(int argc,char**argv){
    assert(argc==2);
    const auto profiles=ulg::emulator_detection::load_profiles(argv[1],"/file-that-does-not-exist");
    ulg::emulator_detection::Detection ps3;
    ps3.status=ulg::emulator_detection::Detection::Status::known_emulator;
    ps3.title_id="PPSA99303";ps3.profile_id="rpcs3ps5";
    const std::string path="/data/homebrew/PPSA99303/rpcs3/games/BLES01181 - Lord of the Rings [EU]";
    auto plan=ulg::emulator_detection::profile_rom_launch_plan("PS3",path,"ps3_ordner",
        "/data/homebrew/PPSA99303",ps3,profiles);
    assert(plan.supported && plan.target=="PPSA99303" && plan.argv.size()==2);
    assert(plan.argv[0]=="--boot");
    assert(plan.argv[1]=="/app0/rpcs3/games/BLES01181 - Lord of the Rings [EU]");
    auto iso=ulg::emulator_detection::profile_rom_launch_plan("PS3",
        "/data/homebrew/PPSA99303/rpcs3/games/example.iso","iso",
        "/data/homebrew/PPSA99303",ps3,profiles);
    assert(iso.supported && iso.argv[1]=="/app0/rpcs3/games/example.iso");
    auto other=ulg::emulator_detection::profile_rom_launch_plan("PS3",
        "/mnt/usb1/ps3/game.iso","iso","/data/homebrew/PPSA99303",ps3,profiles);
    assert(!other.supported);
    auto traversal=ulg::emulator_detection::profile_rom_launch_plan("PS3",
        "/data/homebrew/PPSA99303/rpcs3/games/../private","ps3_ordner",
        "/data/homebrew/PPSA99303",ps3,profiles);
    assert(!traversal.supported);
    auto wrong_format=ulg::emulator_detection::profile_rom_launch_plan("PS3",path,"pkg",
        "/data/homebrew/PPSA99303",ps3,profiles);
    assert(!wrong_format.supported);
    auto wrong=ulg::emulator_detection::profile_rom_launch_plan("Switch",path,"ps3_ordner",
        "/data/homebrew/PPSA99303",ps3,profiles);
    assert(!wrong.supported);
    ps3.status=ulg::emulator_detection::Detection::Status::unidentified_app;
    auto unidentified=ulg::emulator_detection::profile_rom_launch_plan("PS3",path,"ps3_ordner",
        "/data/homebrew/PPSA99303",ps3,profiles);
    assert(!unidentified.supported);

    std::vector<ulg::emulator_detection::Profile> custom;
    assert(ulg::emulator_detection::parse_profiles(R"({"schema":1,"emulators":[{
       "platform":"Wii","id":"fictional","title_id":"PPSA12345","name":"Fictional Emulator",
       "required_files":["eboot.bin"],"rom_launch":{
       "method":"forwarder_argv","args":["--game","{game_path}"],"formats":["wbfs"]
       }}]})",custom));
    ulg::emulator_detection::Detection wii;
    wii.status=ulg::emulator_detection::Detection::Status::known_emulator;
    wii.title_id="PPSA12345";wii.profile_id="fictional";
    auto w=ulg::emulator_detection::profile_rom_launch_plan("Wii","/mnt/usb1/wii/my.wbfs",
       "wbfs","/data/homebrew/PPSA12345",wii,custom);
    assert(w.supported && w.target=="PPSA12345" && w.argv[1]=="/mnt/usb1/wii/my.wbfs");
    assert(!ulg::emulator_detection::parse_profiles(R"({"schema":1,"emulators":[{
       "platform":"Wii","id":"bad","title_id":"PPSA12345","name":"Bad",
       "required_files":["eboot.bin"],"rom_launch":{
       "method":"forwarder_argv","args":["--game","{bad_placeholder}"],"formats":["wbfs"]
       }}]})",custom));
    assert(!ulg::emulator_detection::parse_profiles(R"({"schema":1,"emulators":[{
       "platform":"Wii","id":"bad","title_id":"PPSA12345","name":"Bad",
       "required_files":["eboot.bin"],"rom_launch":{
       "method":"shell_command","args":["/bin/sh","{game_path}"],"formats":["wbfs"]
       }}]})",custom));
}
''')
    subprocess.run(['g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                    '-I', str(NATIVE), str(source), '-o', str(binary)], check=True, timeout=90)
    subprocess.run([str(binary), str(ROOT/'assets/emulator_profiles.json')], check=True, timeout=10)


def test_launcher_connects_profile_to_cross_button_and_details():
    source = (NATIVE/'universal_launcher.cpp').read_text()
    assert source.count('configured_rom_launch(g)') == 2
    assert 'emulator_detection::profile_rom_launch_plan(' in source
    assert 'ulg::send_launch(plan)' in source
    assert 'RPCS3-Start: --boot ueber Forwarder 10199' in source
    assert 'emulator_detection_for(selected_game.platform)' in source
    profile = json.loads((ROOT/'assets/emulator_profiles.json').read_text())
    ps3 = next(e for e in profile['emulators'] if e['platform']=='PS3')
    assert ps3['rom_launch']['args'] == ['--boot', '{game_app0}']
    for e in profile['emulators']:
        if e['platform']!='PS3':
            assert 'rom_launch' not in e
