"""BGH V1.1.32: console-focused emulator setup and modal hints."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()

def test_platform_cards_then_detail_navigation():
    assert 'Waehle deine Konsole' in UI
    assert 'constexpr int cols=4;' in UI
    assert 'emulator_expanded_=true;' in UI
    assert 'emulator_subselected_=0;' in UI
    assert 'if(settings_subview_==0 && emulator_expanded_)' in UI
    assert 'emulator_expanded_=false;' in UI
    assert 'friendly_launch_method' in UI

def test_help_not_stacked_and_browser_readable():
    assert 'if(!settings_ && !browser_open_ && !path_editor_open_ && !search_open_' in UI
    assert 'constexpr int visible=7;' in UI
    assert 'Spieleordner auswaehlen' in UI
    assert 'Diesen Ordner verwenden' in UI
    assert 'begin_emulator_path_editor(app,folder,true)' in UI

def test_preserves_emulator_routes():
    for term in ('stage_ps5sx2_disc_launch(g.path)', 'configured_rom_launch(',
                 'persist_platform_registry(std::move(changed))', 'refresh_emulator_detections()'):
        assert term in UI
