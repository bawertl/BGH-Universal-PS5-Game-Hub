"""BGH V1.1.19 visual update contracts; PS5 hardware remains the validator."""
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()
def slice_between(a,b):return UI.split(a,1)[1].split(b,1)[0]

def test_dualsense_artwork_and_transition_only():
    for name in ('pad_active.png','pad_inactive.png'):
        f=ROOT/'assets/ui'/name
        assert f.is_file()
        with Image.open(f) as img:
            assert img.mode=='RGBA' and img.size==(512,512)
            assert img.getbbox() is not None
    controllers=slice_between('void draw_controllers(', '// Hardware silhouettes are bundled native artwork')
    update=slice_between('void update(const InputFrame', 'void draw(app::Frame &frame)')
    assert 'list.glow(' not in controllers
    assert 'pad_connect_pulse_start_[i]' in controllers
    assert 'elapsed/0.1f' in controllers
    assert 'elapsed<2.0f' in controllers
    assert 'pad_active.png' in controllers and 'pad_inactive.png' in controllers
    assert 'controllers_.observe_primary(input.connected);' in update
    assert 'controllers_.poll();' in update
    assert 'last_pad_visual_state_[slot]' in update
    assert 'scePadOpen(' not in controllers

def test_bar_eight_slots_centered_without_overlap():
    bar=slice_between('void draw_platform_bar(', 'void draw_storage_widgets(')
    assert 'constexpr int visible=8;' in bar
    assert 'rail_x=kFanCentreX-rail_width*0.5f;' in bar
    assert 'if(first+visible<n)' in bar
    assert 360+12+7*147+142 <= 1560
    assert 'sidebar_items_.size()' in bar

def test_recent_cards_wider_and_still_in_safe_canvas():
    cards=slice_between('void draw_recent_cards(', 'void draw_controllers(')
    assert 'recent_card_width=460.0f;' in cards
    assert 'recent_gap=18.0f;' in cards
    assert 'const Rect card{x,915,recent_card_width,110};' in cards
    assert 'shorten(g.title,51)' in cards
    assert 960-(3*460+2*18)/2>=54
    assert 960+(3*460+2*18)/2<=1866
    assert 915+110<1057

def test_storage_theme_and_live_data_untouched():
    storage=slice_between('void draw_storage_widgets(', 'void draw_detail(')
    assert 'glass_card(list,panel,15);' in storage
    assert 'const Color steel=' in storage
    assert 'data.available' in storage and 'data.total' in storage
    assert 'static_cast<double>(data.used)/static_cast<double>(data.total)' in storage
    assert 'ulg::shadow_storage::fetch_storage()' in UI
    assert 'statvfs(' not in UI

def test_fan_artwork_only_and_launch_paths_preserved():
    fan=slice_between('void draw_fan_cover(', 'void draw_cover_fan(')
    assert 'hero_boost=0.31f' in fan
    assert '*264.0f' in fan and '*178.0f' in fan
    assert 'depth<0.55f?42.0f:70.0f' in fan
    for launch in ('stage_ps5sx2_disc_launch(g.path)',
        'make_registered_ps5_launch_plan(g.title_id)',
        'make_registered_ps4_launch_plan(g.title_id)',
        'ulg::send_launch(plan)'):
        assert launch in UI
    assert 'BGH V1.1.22' in UI
    assert 'branches: [bgh-v1.1.22-gallery-ftp]' in WF
