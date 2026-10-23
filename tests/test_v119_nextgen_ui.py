"""NextGen UI: real, read-only telemetry and visually isolated experimental layout."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
STORAGE=(ROOT/'native_ps5/storage_widgets.hpp').read_text()
OVERLAY=(ROOT/'tooling/install_ui_kit_overlay.py').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()

def test_seven_cover_slots_and_single_selection_source():
    fan=UI.split('void draw_cover_fan(',1)[1].split('void draw_game_info(',1)[0]
    assert 'std::array<Candidate,7>' in fan
    assert 'for(int offset=-3;offset<=3;++offset)' in fan
    assert 'draw_fan_cover(list,candidates[i].index)' in fan
    assert 'visual_selection_' in UI
    assert 'indices_[static_cast<std::size_t>(selected_)]' in UI
    assert 'if(indices_.size()>=7)' in UI

def test_real_sandbox_safe_storage_is_read_only():
    assert '#include "storage_widgets.hpp"' in UI
    assert 'statvfs(path,&v)' in STORAGE
    assert 'out.internal=query("/user")' in STORAGE
    for usb in ['/mnt/usb0','/mnt/usb1','/mnt/usb2']:
        assert usb in STORAGE
    assert 'actual.st_dev==base.st_dev' in STORAGE
    assert 'Nicht verfuegbar' in UI
    assert 'next_storage_poll_=clock_+30.0f' in UI
    for forbidden in ['mkdir(', 'system(', 'mount(', 'fopen(', 'remove(', 'popen(']:
        assert forbidden not in STORAGE

def test_tabs_and_buttons_still_use_existing_platforms():
    assert 'void draw_platform_bar(' in UI
    assert 'sidebar_items_[static_cast<std::size_t>(index)]' in UI
    assert 'sidebar_profile_visible(p)' in UI
    assert 'if(input.is_pressed(Action::page_prev))' in UI
    assert 'if(input.is_pressed(Action::page_next))' in UI
    assert 'void draw_settings(' in UI
    assert 'void request_selected_launch(' in UI
    assert 'void draw_recent_cards(' in UI
    assert 'music_settings_.enabled' in UI

def test_feature_is_isolated_and_header_installed():
    assert 'branches: [bgh-v1.1.9-nextgen-ui]' in WF
    assert "storage_widgets.hpp', concepts/'storage_widgets.hpp'" in OVERLAY
    assert 'BGH V1.1.9-EMULATOR-AUTODETECTION' in UI
    assert 'UI-TEST' in UI
