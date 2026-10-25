"""V1.0.8 regression guards for safe cosmetic and emulator-entry fixes."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "native_ps5/universal_launcher.cpp").read_text()
MONITOR = (ROOT / "native_ps5/controller_status.hpp").read_text()
WF = (ROOT / ".github/workflows/ps5-build.yml").read_text()


def between(source, a, b):
    return source.split(a, 1)[1].split(b, 1)[0]


def test_list_header_is_free_standing_and_separated():
    header = between(UI, 'void draw(app::Frame &frame) const', 'void request_selected_launch(')
    assert 'const Rect list_header' not in header
    assert 'draw_platform_bar(list);' in header
    assert 'draw_storage_widgets(list);' in header
    assert 'draw_cover_fan(list);' in header
    assert 'draw_platform_bar(list);' in header
    assert 'constexpr float kListTop = 192.0f;' in UI


def test_platform_as_text_and_real_source_ids():
    game = between(UI, 'void draw_game_info(', 'std::array<int,3> recent_indices()')
    assert 'chip_w' not in game and 'chip_padding' not in game
    assert 'serif_text(list,label,platform_x,803,19' in game
    assert 'const Rect launch_button{870,819,180,45};' in game
    for platform in ('PS2','PS4','PS5','Switch','Xbox 360','Homebrew'):
        assert 'platform=="'+platform+'"' in UI
    assert 'if(platform=="PS2" || platform=="PlayStation")' not in UI


def test_only_hero_glows_and_unverified_battery_is_hidden():
    fan = between(UI, 'void draw_fan_cover(', 'void draw_cover_fan(')
    assert 'if(depth<0.18f)' in fan
    assert '0x00E5FF,0.49f*pulse' in fan
    assert 'if(depth<0.55f)\n            list.glow(' not in fan
    controller = between(UI, 'void draw_controllers(', 'void draw_platform_bar(')
    assert 'const int battery=controllers_.battery[i];' in controller
    assert 'if(battery>=0 && battery<=100)' in controller
    assert 'std::string("--")' not in controller
    assert 'sample[0x54]' not in MONITOR


def test_settings_have_supported_native_glyphs_and_translation():
    settings = between(UI, 'void draw_settings(app::Frame &frame)', 'void draw_settings_roots(')
    for glyph in ('l1','r1','dpad','cross','circle'):
        assert 'ui::Button::'+glyph in settings
    assert 'ui::draw_hints(' in settings
    assert '"L1/R1: Tabs   |' not in settings
    loc = (ROOT / 'native_ps5/ui_localization.hpp').read_text()
    assert '{"Wechseln","Change"}' in loc
    shim = (ROOT / 'tooling/host_shim/concepts/concepts.hpp').read_text()
    assert 'dpad' in shim


def test_known_emulator_apps_use_menu_without_rom_or_history():
    start = between(UI, 'void request_selected_launch(', 'void refresh_storage(')
    menu = between(start, 'if(selected_game.platform=="Homebrew")', '// Menu-only mode never stages')
    for title_id in ('PPSA99203','PPSA99008','PPSA50011'):
        assert title_id in UI
    assert 'emulator_platform_for_title(selected_game.title_id)' in menu
    assert 'menu_plan=emulator_menu_plan(known_platform)' in menu
    assert 'ulg::send_launch(menu_plan)' in menu
    assert 'd.status==ulg::emulator_detection::Detection::Status::known_emulator' in menu
    assert 'd.title_id==selected_game.title_id' in menu
    assert 'record_recent(' not in menu
    assert 'stage_ps5sx2_disc_launch(' not in menu
    assert 'make_launch_plan(' not in menu
    assert 'return;' in menu
    for old in ('make_registered_ps4_launch_plan(g.title_id)',
                'make_registered_ps5_launch_plan(g.title_id)',
                'stage_ps5sx2_disc_launch(g.path)'):
        assert old in start


def test_release_consistency():
    assert 'BGH V1.1.26' in UI
    assert 'release_label="BGH V1.1.9"' in UI
    assert 'branches: [bgh-v1.1.26-fix1]' in WF
    assert 'BGH-V1-1-26-PPSA99079' in WF
    assert 'V1_1_26_${TITLE_ID}_PS5_TEST.zip' in (ROOT/'tooling/build_native_ps5.sh').read_text()
