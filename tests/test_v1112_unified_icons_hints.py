"""V1.1.3: 12 consistent icons, input legends use actual PS button glyphs."""
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
LOC=(ROOT/'native_ps5/ui_localization.hpp').read_text()
GLYPHS=(ROOT/'tooling/host_shim/concepts/concepts.hpp').read_text()
NAMES=['ps5','ps4','ps2','switch','xbox360','ps1','psp','ps3','xbox','dreamcast','gamecube','wii']

def test_all_platform_icons_uniform_and_alpha():
    for name in NAMES:
        path=ROOT/'assets/platforms'/f'{name}.png'
        with Image.open(path) as img:
            assert img.mode=='RGBA'
            assert img.size==(96,96)
            assert img.getpixel((0,0))[3]==0
            assert img.getchannel('A').getbbox()

def test_unambiguous_emulator_hints_by_context():
    assert 'draw_emulator_button_legend(list)' in SRC
    assert 'ui::Button::touchpad,hint("Pfad eingeben")' in SRC
    assert 'ui::Button::triangle,hint("Entfernen")' in SRC
    assert 'ui::Button::square,hint("Einklappen")' in SRC
    assert 'ui::Button::l2,hint("Reihenfolge"),ui::Button::r2' in SRC
    assert 'ui::Button::cross,hint("AN/AUS")' in SRC
    assert 'ui::Button::cross,hint(app?"Anwendung bearbeiten"' in SRC
    assert 'ui::Button::circle,hint("Schliessen")' in SRC
    assert 'ui::Button::dpad,hint("Auswahl")' in SRC
    assert 'enum class Button {cross,circle,l1,r1,l2,r2' in GLYPHS
    assert '{"Pfad eingeben","Enter Path"}' in LOC

def test_folder_browser_and_editor_use_glyphs():
    browser=SRC.split('void draw_browser(app::Frame &frame) const {',1)[1].split('void draw_path_editor(',1)[0]
    assert 'ui::Button::touchpad,hint("Nach oben")' in browser
    assert 'ui::Button::square,hint("Hier hinzufuegen")' in browser
    assert 'ui::Button::triangle,hint("Pfad eingeben")' in browser
    editor=SRC.split('void draw_path_editor(app::Frame &frame) const {',1)[1]
    assert 'ui::Button::square,hint("Loeschen")' in editor
    assert 'ui::Button::circle,hint("Zurueck")' in editor

def test_version_and_branch():
    wf=(ROOT/'.github/workflows/ps5-build.yml').read_text()
    assert 'branches: [bgh-v1.1.5]' in wf
    assert 'BGH-V1-1-5-COVER-PPSA99079-TEST' in wf
    assert 'BGH V1.1.5-CONTAINER-COVER-FIX' in SRC
