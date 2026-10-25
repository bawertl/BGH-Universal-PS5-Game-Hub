"""V1.0.5 UI and controller regression guards; no synthetic PS5 battery data."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
MONITOR=(ROOT/'native_ps5/controller_status.hpp').read_text()
LIB=(ROOT/'native_ps5/library_core.cpp').read_text()
WORKFLOW=(ROOT/'.github/workflows/ps5-build.yml').read_text()


def test_version_consistency():
    assert 'BGH V1.1.26' in UI
    assert 'release_label="BGH V1.1.9"' in UI
    assert 'branches: [bgh-v1.1.26-fix1]' in WORKFLOW
    assert 'BGH-V1-1-26-PPSA99079' in WORKFLOW
    assert 'V1_1_26_${TITLE_ID}_PS5_TEST.zip' in (ROOT/'tooling/build_native_ps5.sh').read_text()


def test_7_cover_fan_and_bolder_title():
    section=UI.split('void draw_cover_fan(gfx::DrawList &list) const {',1)[1].split('void draw_game_info(',1)[0]
    assert 'std::array<Candidate,7> candidates' in section
    assert 'offset=-3;offset<=3' in section
    assert 'depth*18.0f' in UI
    assert 'min(depth,1.0f)*215.0f' in UI
    assert 'depth-1.0f)*145.0f' in UI
    assert 'serif_text(list,title,title_x,778,size,Color::rgb(0xF0F8FF),1320);' in UI


def test_real_connected_glow_and_unverified_battery():
    block=UI.split('void draw_controllers(gfx::DrawList &list) const {',1)[1].split('// Hardware silhouettes',1)[0]
    assert 'const bool connected=(status==1);' in block
    assert '0xF5E2BC,0.18f' in block
    assert '0x6A7588,0.38f' in block
    assert 'list.rounded_rect({x+43,y+35,8,3}' in block
    assert 'const int battery=controllers_.battery[i];' in block
    assert 'if(battery>=0 && battery<=100)' in block
    assert 'controllers_.charging[i]==1' in block
    assert 'battery.fill(-1);' in MONITOR and 'charging.fill(-1);' in MONITOR
    assert 'sample[0x54]' not in MONITOR and 'sample[0x55]' not in MONITOR
    assert 'controllers_.observe_primary(input.connected);' in UI


def test_display_title_cleanup_preserves_real_names(tmp_path):
    assert 'game.title=ulg::clean_title_for_display(game.title);' in UI
    assert 'std::string clean_title_for_display' in LIB
    harness=tmp_path/'titles.cpp'
    harness.write_text('''
    #include "library_core.hpp"
    #include <cassert>
    int main(){
      using ulg::clean_title_for_display;
      assert(clean_title_for_display("Mario Kart 8 Deluxe (nsw2u)")=="Mario Kart 8 Deluxe");
      assert(clean_title_for_display("Zelda [0100AABBCCDDEEFF] [v1.2.3]")=="Zelda");
      assert(clean_title_for_display("Luigi's Mansion 3 [T-En]")=="Luigi's Mansion 3");
      assert(clean_title_for_display("Pokémon (USA) (Remastered)")=="Pokémon (Remastered)");
      assert(clean_title_for_display("Game (Director's Cut)")=="Game (Director's Cut)");
      assert(clean_title_for_display("Part 2 (Episode 3)")=="Part 2 (Episode 3)");
      assert(clean_title_for_display("Mario Kart 8 Deluxe")=="Mario Kart 8 Deluxe");
    }
    ''')
    exe=tmp_path/'titles'
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror',
        '-I',str(ROOT/'native_ps5'),str(harness),str(ROOT/'native_ps5/library_core.cpp'),
        '-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
