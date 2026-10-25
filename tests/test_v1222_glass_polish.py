"""V0.12.23: visual-only changes must keep the real PS5 glyph and launch handlers."""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/'native_ps5/universal_launcher.cpp').read_text(encoding='utf-8')
WORKFLOW=(ROOT/'.github/workflows/ps5-build.yml').read_text(encoding='utf-8')

def test_fan_moves_left_and_selected_cover_is_emphasised():
    assert 'kFanCentreX = (kGameListRight + (kScreenRight-kFanRightMargin))*0.5f' in SRC
    assert 'hero_boost=0.19f*std::max(0.0f,1.0f-depth*0.88f)' in SRC
    assert 'draw_cover_fan(list)' in SRC
    assert 'fan_cover_header_asset(g.platform)' in SRC

def test_info_is_free_standing_and_uses_ps_glyph():
    main=SRC.split('void draw_game_info(gfx::DrawList &list) const {',1)[1].split('std::array<int,3> recent_indices()',1)[0]
    assert 'glass_card(list,{742,621,500,146},22)' not in SRC
    assert 'ui::Button::cross,hint(menu_mode?"Menue":"Spielen")' in main
    assert 'ui::draw_hints(' in main
    assert 'Verfügbar' not in main
    assert 'Installationsordner' not in main
    assert 'Start testen' not in main
    assert 'X  ' not in main
    # V1.0.8 intentionally removes the platform capsule while keeping the label.
    assert 'serif_text(list,label,platform_x,803,19' in main
    assert 'chip_w' not in main and 'chip_padding' not in main

def test_launch_paths_remain_present():
    assert 'stage_ps5sx2_disc_launch(g.path)' in SRC
    assert 'ulg::send_launch(plan)' in SRC
    assert 'make_registered_ps4_launch_plan(g.title_id)' in SRC
    assert 'make_registered_ps5_launch_plan(g.title_id)' in SRC

def test_build_versions_match():
    assert 'bgh-v1.1.30' in WORKFLOW
    assert 'BGH V1.1.30' in SRC
    assert 'V1_1_30_PPSA99079_PS5_TEST.zip' in WORKFLOW
    assert 'V1_1_30_' in (ROOT/'tooling/build_native_ps5.sh').read_text()
    assert 'V0.12.21' not in WORKFLOW
    assert 'bgh-v1221' not in WORKFLOW
