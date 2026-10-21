"""V1.0.5 UI block 2: measured button, header spacing, and restored Switch badge."""
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
WORKFLOW=(ROOT/'.github/workflows/ps5-build.yml').read_text()

def test_launch_button_content_is_measured_and_centered():
    block=SRC.split('void draw_game_info(gfx::DrawList &list) const {',1)[1].split('    std::array<int,3> recent_indices()',1)[0]
    assert 'ui::measure_hints(fonts,start_hint,1,start_layout)' in block
    assert 'const float start_x=launch_button.x+(launch_button.w-start_width)*0.5f;' in block
    assert 'start_layout.cy=launch_button.y+launch_button.h*0.5f;' in block
    assert 'start_hint,1,start_x,false,start_layout' in block
    assert 'list.gradient_rect(launch_button,18,' in block
    assert 'ui::Button::cross,hint(menu_mode?"Menue":"Spielen")' in block

def test_list_header_clears_first_row():
    # V1.0.8 uses a divider and no outer header capsule.
    assert 'const Rect list_header' not in SRC
    assert 'list.rounded_rect({300,176,386,2},1' in SRC
    assert 176+2+6 < 192
    assert 'constexpr float kListTop = 192.0f;' in SRC

def test_switch_badge_restores_complete_left_icon():
    with Image.open(ROOT/'assets/ui/fan_case_header_switch.png') as image:
        assert image.mode=='RGBA' and image.size==(900,220)
        # The recovered original badge has both Joy-Con halves; visible content
        # begins well inside the asset instead of being clipped at pixel zero.
        assert image.getchannel('A').getbbox()[0]>=8
        # At x=50 there must be visible white glyph art in the red badge.
        patch=image.crop((25,50,150,180))
        # Use stable raw RGBA bytes: works on both Ubuntu's older Pillow and
        # newer Pillow releases, without version-specific image iterators.
        rgba=patch.tobytes()
        assert any(rgba[i]>215 and rgba[i+1]>215 and rgba[i+2]>215 and rgba[i+3]>150
                   for i in range(0,len(rgba),4))
    assert 'fan_cover_header_asset(g.platform)' in SRC

def test_release_1_0_2_is_consistent():
    build=(ROOT/'tooling/build_native_ps5.sh').read_text()
    assert 'branches: [bgh-v1.1.8, main]' in WORKFLOW
    assert 'BGH V1.1.8-PLAYTIME-PER-GAME-SAFETY' in SRC and 'BGH V1.1.8-PLAYTIME-PER-GAME-SAFETY' in WORKFLOW
    assert 'BGH-V1-1-8-PLAYTIME-PPSA99079-TEST' in WORKFLOW
    assert 'V1_1_8_PPSA99079_PS5_TEST.zip' in WORKFLOW
    assert 'V1_1_8_${TITLE_ID}_PS5_TEST.zip' in build
