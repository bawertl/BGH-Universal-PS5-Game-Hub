"""V1.1.8: emulator app runtime is never shown as a ROM's playtime."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / 'native_ps5/universal_launcher.cpp').read_text()
HEADER = (ROOT / 'native_ps5/playtime_log.hpp').read_text()


def test_each_emulated_game_is_not_assigned_emulator_process_total():
    start = UI.index('std::string native_log_time_label(const ulg::Game &g) const')
    end = UI.index('void set_message(std::string message)', start)
    helper = UI[start:end]
    assert 'is_native_game_id(g.platform,g.title_id)' in helper
    assert 'g.title_id' in helper
    code_only='\n'.join(line for line in helper.splitlines() if not line.lstrip().startswith('//'))
    assert 'PPSA99203' not in code_only
    assert 'PPSA99008' not in code_only
    assert 'PPSA50011' not in code_only
    assert 'if(platform=="PS4")' in HEADER
    assert 'return false;' in HEADER
    assert 'ROM-Spielzeit noch nicht messbar' in UI
    assert 'Emulator-Laufzeit (Log):' not in UI


def test_playtime_label_next_to_platform_no_detail_path_reflow():
    hero = UI[UI.index('void draw_game_info('):UI.index('void draw_cover_fan(', UI.index('void draw_game_info('))] if 'void draw_cover_fan(' in UI[UI.index('void draw_game_info('):] else UI[UI.index('void draw_game_info('):UI.index('void draw_detail(')]
    assert 'serif_text(list,platform_text,755,698,20' in hero
    assert 'const float time_x=755.0f+serif_text_width(platform_text,20.0f)+42.0f;' in hero
    assert 'serif_text(list,native_time,time_x,698,24' in hero
    detail = UI[UI.index('void draw_detail('):UI.index('void refresh_playtime_log(')]
    assert 'translated_text(list,fonts.semibold,native_detail_time,1060,309,28' in detail
    assert 'shorten(playtime_detail_label(g),73)' not in detail
    assert 'Spielzeit:' in UI
    assert 'Log-Spielzeit:' not in UI


def test_shadowmount_log_is_bounded_and_not_scanned_per_frame():
    assert 'refresh_playtime_log();' in UI
    assert 'const std::string native_time=native_log_time_label(g);' in UI
    assert 'ftp.retrieve(' in UI
    assert 'kMaxLogBytes=2*1024*1024' in HEADER
    assert 'fopen(path,"wb")' not in HEADER
