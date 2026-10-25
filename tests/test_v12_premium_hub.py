"""BGH V0.12: executable navigation math plus native layout/regression guard."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / "native_ps5" / "universal_launcher.cpp"
HEADER = ROOT / "native_ps5" / "premium_navigation.hpp"


def test_navigation_model_builds_and_handles_unbounded_lists(tmp_path):
    harness=tmp_path / "nav.cpp"
    harness.write_text(r'''
#include "premium_navigation.hpp"
#include <cassert>
#include <cmath>
int main(){
  using namespace ulg::premium;
  assert(step(0,-1,25)==0);
  assert(step(0,1,25)==1);
  assert(step(24,1,25)==24);
  assert(step(99,1,25)==24);
  assert(step(0,1,0)==0);
  assert(scroll_target(0,10000,9)==0.0f);
  assert(scroll_target(500,10000,9)==496.0f);
  assert(scroll_target(9999,10000,9)==9991.0f);
  assert(scroll_target(3,7,9)==0.0f);
  float p=1.0f;
  p=approach(p,2,0.016f,13.5f);
  assert(p>1 && p<2);
  assert(approach(p,2000,0.016f,13.5f)==2000);
  assert(approach(NAN,5,0.016f,13.5f)==5);
  assert(approach(3,3,0.016f,13.5f)==3);
}
''')
    exe=tmp_path / "nav"
    subprocess.run(["g++","-std=c++20","-Wall","-Wextra","-Werror",
        "-I",str(HEADER.parent),str(harness),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_single_game_selection_and_real_library_drives_both_views():
    source=CPP.read_text()
    assert "const auto &g=catalogue_.games[indices_[static_cast<std::size_t>(selected_)]];" in source
    assert "draw_game_info(list);" in source
    assert "draw_cover_fan(list);" in source
    assert "draw_game_info(list);" in source
    assert "draw_platform_bar(list);" in source
    assert "visual_selection_=" in source
    assert "selected_=next" in source
    assert "kVisibleListRows = 9" in source
    assert "std::array<Candidate,7> candidates" in source
    assert "counts_.fill(0)" in source and "++counts_[0]" in source
    assert "No game paths or installations are modified" in source
    assert "launch_registered_ps5" in source
    assert "ulg::send_launch(plan)" in source
    assert "kEmulatorCoverFolder" in source
    assert "attempted_emulator_covers_" in source
    assert "draw_detail(frame)" in source
    assert "draw_search(frame)" in source
    assert "draw_settings(frame)" in source
    assert "draw_card(" not in source
    assert "fan_case_header_ps5" in source
    assert "uv_cover_fit(" in source


def test_package_sources_keep_audio_logo_path_and_workflow():
    # Launcher header needs transparent badge; PS5 Home tile needs opaque artwork.
    from PIL import Image
    with Image.open(ROOT/"assets/bgh_logo.png") as badge, Image.open(ROOT/"sce_sys/icon0.png") as tile:
        assert badge.size==tile.size==(512,512)
        assert badge.mode=="RGBA" and tile.mode=="RGB"
    source=CPP.read_text()
    assert "audio::Cue::focus" in source
    assert "audio::Cue::launch" in source
    assert "audio::SoundSet::glass" in source
    overlay=(ROOT/"tooling/install_ui_kit_overlay.py").read_text()
    assert "premium_navigation.hpp" in overlay
    assert "shell.cpp" in overlay
    script=(ROOT/"tooling/build_native_ps5.sh").read_text()
    assert "make -C \"$KIT\"" in script
    assert "ps5-homebrew-ui" in script
    assert "PPSA99079" in script
    assert "V1_1_9" in script
    workflow=(ROOT/".github/workflows/ps5-build.yml").read_text()
    assert "bgh-v1.1.26-fix1" in workflow
    assert "BGH-V1-1-26" in workflow
