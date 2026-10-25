"""V1.1.9: read-only emulator detection, configurable profiles and safe new ROM scan."""
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / 'native_ps5/universal_launcher.cpp').read_text()


def test_app_folder_detection_with_realistic_metadata_and_profiles(tmp_path):
    app = tmp_path / 'PPSA99303'
    (app / 'sce_sys').mkdir(parents=True)
    (app / 'eboot.bin').write_bytes(b'non-executable-host-fixture')
    (app / 'rpcs3ps5-launch.elf').write_bytes(b'fixture')
    (app / 'sce_sys/param.json').write_text(json.dumps({
        'titleId': 'PPSA99303', 'localizedParameters': {
            'defaultLanguage': {'titleName': 'RPCS3 (Homebrew)'}}}))
    bad = tmp_path / 'PPSA11111'
    (bad / 'sce_sys').mkdir(parents=True)
    (bad / 'eboot.bin').write_bytes(b'fixture')
    (bad / 'sce_sys/param.json').write_text('{"titleId":"PPSA99999"}')
    harness = tmp_path / 'probe.cpp'
    harness.write_text(r'''
#include "emulator_detection.hpp"
#include <cassert>
#include <fstream>
#include <string>
int main(int argc,char** argv){
    assert(argc==4);
    const std::string app=argv[1],bad=argv[2],asset=argv[3];
    auto profiles=ulg::emulator_detection::load_profiles(asset,"/definitely-not-present/custom.json");
    assert(profiles.size()>=4);
    const auto a=ulg::emulator_detection::detect(app,"PS3",profiles);
    assert(a.status==ulg::emulator_detection::Detection::Status::known_emulator);
    assert(a.title_id=="PPSA99303" && a.profile_id=="rpcs3ps5");
    assert(ulg::emulator_detection::description(a).find("Menue-Start ungetestet")!=std::string::npos);
    const auto wrong=ulg::emulator_detection::detect(app,"Wii",profiles);
    assert(wrong.status==ulg::emulator_detection::Detection::Status::unidentified_app);
    const auto invalid=ulg::emulator_detection::detect(bad,"PS3",profiles);
    assert(invalid.status==ulg::emulator_detection::Detection::Status::mismatch);
    const auto missing=ulg::emulator_detection::detect("/path-which-does-not-exist","PS3",profiles);
    assert(missing.status==ulg::emulator_detection::Detection::Status::invalid_path);
    const auto absent=ulg::emulator_detection::detect("","PS3",profiles);
    assert(absent.status==ulg::emulator_detection::Detection::Status::no_path);
    std::vector<ulg::emulator_detection::Profile> parsed;
    assert(!ulg::emulator_detection::parse_profiles("{garbage",parsed));
    assert(!ulg::emulator_detection::parse_profiles(R"({"schema":1,"emulators":[{"platform":"PS3","id":"injected","title_id":"PPSA12345","name":"Bad","required_files":["../evil"]}]})",parsed));
    assert(ulg::emulator_detection::parse_profiles(R"({"schema":1,"emulators":[{"platform":"Wii","id":"dolphin","title_id":"PPSA12345","name":"Dolphin","required_files":["eboot.bin"]}]})",parsed));
    assert(parsed.size()==1 && parsed[0].platform=="Wii");
}
''')
    binary = tmp_path / 'probe'
    subprocess.run(['g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                    '-I', str(ROOT / 'native_ps5'), str(harness), '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(app), str(bad), str(ROOT / 'assets/emulator_profiles.json')], check=True)


def test_non_core_platform_roms_scanned_without_launch_claims(tmp_path):
    root = tmp_path / 'roms'
    root.mkdir()
    entries = {'PS3': 'Example PS3.iso', 'Xbox': 'Example Xbox.xbe',
               'Dreamcast': 'Example DC.gdi', 'GameCube': 'Example GC.rvz',
               'Wii': 'Example Wii.wbfs'}
    paths = tmp_path / 'paths.conf'
    lines = []
    for platform, name in entries.items():
        folder = root / platform
        folder.mkdir()
        (folder / name).write_bytes(b'rom-test-fixture')
        lines.append(f'{platform}|{folder}')
    paths.write_text('\n'.join(lines) + '\n')
    out = tmp_path / 'catalogue.json'
    subprocess.run([str(ROOT / 'native_ps5/scanner-host'), '--paths', str(paths),
                    '--output', str(out)], check=True, capture_output=True)
    games = json.loads(out.read_text())['games']
    names = {(g['platform'], g['path']) for g in games}
    for platform, name in entries.items():
        assert (platform, str(root / platform / name)) in names
    # Recognition of ROM filenames does NOT imply the emulator can boot them.
    reg = (ROOT / 'native_ps5/platform_registry.hpp').read_text()
    for platform in ('PS3', 'Xbox', 'Dreamcast', 'GameCube', 'Wii'):
        assert f'add("{platform}"' in reg
        assert 'true,true,false,false' in reg


def test_no_unverified_rom_launch_and_overlay_asset_present():
    assert 'emulator_detection::load_profiles()' in CPP
    assert 'test_emulator_menu(profile,feedback)' in CPP
    assert 'plan.target=d.title_id;plan.supported=true' in CPP
    assert 'request_selected_launch(feedback)' in CPP
    overlay = (ROOT / 'tooling/install_ui_kit_overlay.py').read_text()
    assert "emulator_detection.hpp" in overlay
    assert "emulator_profiles.json" in overlay
    workflow = (ROOT / '.github/workflows/ps5-build.yml').read_text()
    assert 'bgh-v1.1.27' in workflow
    assert 'emulator_detection.hpp' in workflow
