"""Fix2: no stale USB games; existing read-only path browser is reachable."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()

def test_usb_cache_is_not_merged_into_fresh_scan():
    assert 'catalogue_=std::move(fresh);' in UI
    assert 'catalogue_=ulg::merge_unavailable(std::move(fresh),catalogue_);' not in UI
    assert 'ulg::write_catalogue_cache(catalogue_,kCacheFile)' in UI

def test_emulator_folder_opens_browser_and_manual_mode_remains():
    assert 'begin_emulator_path_editor(app,folder,false);' in UI
    assert 'begin_emulator_path_editor(app,folder,true);' in UI
    assert 'begin_emulator_path_editor(false,profile.rom_folders.size(),false);' in UI
    assert 'browser_=ulg::browse_folders(browser_path_)' in UI
    assert 'save_emulator_path(browser_path_)' in UI
    assert 'ui::Button::square,hint("Hier hinzufuegen")' in UI

def test_usb_mount_poll_is_rate_limited_and_branch_matches():
    assert 'next_media_probe_=clock_+12.0f;' in UI
    assert 'if(changed)refresh();' in UI
    assert 'branches: [bgh-v1.1.30-fix2]' in WF
    assert 'test "$GITHUB_REF_NAME" = \'bgh-v1.1.30-fix2\'' in WF
