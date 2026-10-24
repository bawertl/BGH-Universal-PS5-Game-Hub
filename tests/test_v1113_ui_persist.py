"""V1.1.14 targeted regression checks: does not claim PS5 hardware success."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
PAD=(ROOT/'native_ps5/controller_status.hpp').read_text()
BUILD=(ROOT/'tooling/build_native_ps5.sh').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()

def test_gallery_restore_in_regular_update_not_enter_or_settings():
    assert 'if(!gallery_selection_loaded_after_boot_)' in UI
    assert 'settings_ && settings_tab_==3 && !gallery_selection_loaded_after_boot_' not in UI
    part=UI.split('void update(const InputFrame &input, float dt, app::Feedback &feedback) override',1)[1].split('void draw(app::Frame &frame) const override',1)[0]
    assert 'ulg::bgh_gallery::load_selection' in part
    assert 'refresh_gallery_paths();' in part
    assert part.index('load_selection')<part.index('if(clock_>=next_storage_poll_)')

def test_eight_pills_fit_and_count_is_last():
    bar=UI.split('void draw_platform_bar(',1)[1].split('void draw_storage_widgets(',1)[0]
    assert 'constexpr int visible=8;' in bar
    assert 'pill_width=142.0f' in bar
    assert 'pill_step=147.0f' in bar
    assert 'const float x=rail_x+12.0f+slot*pill_step;' in bar
    assert 'std::string display=label+suffix;' in bar
    assert 'const std::string suffix=" | "+count;' in bar
    assert 'label.pop_back();' in bar
    assert 360+12+7*147+142 <= 1560

def test_pad_diagnostics_do_not_add_probe_or_unverified_state():
    assert 'last_get' in PAD and 'last_open' in PAD and 'last_read' in PAD
    assert 'probe_summary' in PAD
    assert 'read_pad(initial_user,0,index,index)' in PAD
    assert 'read_pad(user,0,0,free_slot)' in PAD
    assert 'sample+0x4c' in PAD # still only a candidate, not certified by source-level tests
    assert '0x54' not in PAD
    assert 'controllers_.diagnostic_pad(1)' in UI
    assert 'controllers_.diagnostic_users()' in UI

def test_storage_and_build_startup_guards_retained():
    assert 'statvfs(' not in UI
    assert 'statvfs(' not in (ROOT/'native_ps5/bgh_storage_metrics.hpp').read_text()
    assert 'V1_1_19_${TITLE_ID}_PS5_TEST.zip' in BUILD
    assert 'bgh-v1.1.19-visual-polish' in WF
    assert 'PPSA99079' in WF
