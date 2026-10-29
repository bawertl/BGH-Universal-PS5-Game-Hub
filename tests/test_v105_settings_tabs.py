"""V1.0.5: three settings tabs, safe routing, and unchanged persistent data."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / 'native_ps5/universal_launcher.cpp').read_text()
WORKFLOW = (ROOT / '.github/workflows/ps5-build.yml').read_text()
SHIM = (ROOT / 'tooling/host_shim/concepts/concepts.hpp').read_text()


def section(start: str, end: str) -> str:
    return CPP.split(start, 1)[1].split(end, 1)[0]


def test_three_tabs_and_settings_scoped_navigation():
    assert 'constexpr std::array<const char *,3> kSettingsTabs' in CPP
    for label in ('Emulatoren & Spielordner', 'Darstellung & Verhalten', 'System & Hilfe'):
        assert f'"{label}"' in CPP
    update = section('void update_settings(', 'void refresh_browser()')
    assert 'settings_tab_=(settings_tab_+count+step)%count;' in update
    assert 'if(settings_tab_!=3)return;' in update
    assert 'Action::page_prev' in update and 'Action::page_next' in update
    assert 'Action::jump_prev' in update and 'Action::jump_next' in update
    assert update.index('if(settings_tab_!=3)return;') < update.index('root.enabled=!root.enabled;')
    assert update.index('if(settings_tab_!=3)return;') < update.index('browser_open_=true;')
    assert 'if(settings_){update_settings(input,feedback);return;}' in CPP
    assert 'if(settings_){settings_tab_=0;probe_recent_storage();}' in CPP
    assert 'jump_prev,jump_next' in SHIM


def test_three_separate_views_and_unmodified_path_helpers():
    draw=section('void draw_settings(app::Frame &frame) const {','void draw_browser(')
    assert 'draw_settings_roots(list)' not in draw
    assert 'draw_root_button_legend(list)' not in draw
    assert 'draw_settings_emulators(list)' in draw
    assert 'draw_settings_presentation(list)' in draw
    assert 'draw_settings_system(list)' in draw
    # V1.0.8 replaces ASCII instructions with native DualSense hints.
    assert 'ui::Button::l1' in draw and 'ui::Button::r1' in draw
    assert 'ui::Button::dpad' in draw
    assert 'ui::draw_hints(' in draw
    assert 'Filter: '+ '"+filter+" (L2/R2)' in draw
    for text in ('save_roots_and_rescan();', 'refresh();', 'browser_open_=true;',
                 'path_editor_open_=true;', 'settings_visible_roots()'):
        assert text in CPP
    assert 'void update_browser(const InputFrame &input' in CPP
    browser=section('void update_browser(', 'void update_path_editor(')
    assert 'Action::page_prev' in browser and 'Action::page_next' in browser
    assert 'VERLAUF-DIAG  PRIMARY=' in draw
    assert 'GESPEICHERTE REFERENZEN=' in draw


def test_language_selectable_and_emulator_mode_is_separate():
    presentation=section('void draw_settings_presentation(', 'void draw_settings_system(')
    assert 'Deutsch' in presentation
    assert 'English' in presentation
    assert 'Emulator-Startverhalten' in presentation
    assert 'save_emulator_boot_setting(' in CPP
    assert 'Nur Emulator-Menue' in presentation
    assert 'settings_tab_' not in (ROOT/'native_ps5/state_store.hpp').read_text()
    assert 'settings_tab_' not in (ROOT/'native_ps5/state_store.cpp').read_text()
    assert 'kPrimaryPrefsFile' in CPP and 'kFallbackPrefsFile' in CPP


def test_version_and_workflow_match():
    assert 'BGH V1.1.30' in CPP
    assert 'release_label="BGH V1.1.9"' in CPP
    assert 'branches: [bgh-v1.1.30-fix3]' in WORKFLOW
    assert 'BGH-V1-1-30-FIX3-PPSA99079' in WORKFLOW
    assert 'V1_1_30_PPSA99079_PS5_TEST.zip' in WORKFLOW
    assert 'V1_1_30_${TITLE_ID}_PS5_TEST.zip' in (ROOT / 'tooling/build_native_ps5.sh').read_text()
