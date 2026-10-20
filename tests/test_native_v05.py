"""V0.5: cover-format precedence and deterministic, non-destructive kit overlay."""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native_ps5'


def test_png_cover_preferred_over_unsupported_jpeg(tmp_path):
    game_folder = tmp_path / 'ps2'
    game_folder.mkdir()
    (game_folder / 'WRC.iso').write_bytes(b'not an actual game; scanner only')
    (game_folder / 'cover.jpg').write_bytes(b'fake-jpeg')
    (game_folder / 'folder.png').write_bytes(b'fake-png')
    config = tmp_path / 'paths.conf'
    config.write_text('PS2|' + str(game_folder) + '\n')
    result = tmp_path / 'library.json'
    subprocess.run(['make', '-C', str(NATIVE), 'scanner-host'], check=True, capture_output=True)
    subprocess.run([str(NATIVE / 'scanner-host'), '--paths', str(config), '--output', str(result)], check=True)
    games = json.loads(result.read_text())['games']
    assert len(games) == 1
    assert games[0]['cover_path'] == str(game_folder / 'folder.png')


def test_ui_overlay_is_repeatable_and_preserves_originals(tmp_path):
    kit = tmp_path / 'ui_kit'
    concepts = kit / 'src' / 'concepts'
    concepts.mkdir(parents=True)
    (kit / 'src' / 'app').mkdir(parents=True)
    (kit / 'assets').mkdir()
    (concepts / 'concepts.hpp').write_text('namespace hui::concepts {\n} // namespace hui::concepts\n')
    (concepts / 'registry.cpp').write_text('// original registry\n')
    shell = kit / 'src' / 'app' / 'shell.cpp'
    shell.write_text('''void update() {
if (input.is_pressed(Action::page_next) || input.is_pressed(Action::page_prev)) {}
if (input.is_pressed(Action::touch)) {}
forwarded.pressed &= ~kShellActions;
forwarded.released &= ~kShellActions;
forwarded.held &= ~kShellActions;
}
''')
    (kit / 'Makefile').write_text('OPENGL_SDK := .deps/ps5-opengl/current\n')
    script = ROOT / 'tooling' / 'install_ui_kit_overlay.py'
    for _ in range(2):
        subprocess.run([sys.executable, str(script), str(kit)], check=True, capture_output=True)
    assert (kit / 'src' / 'app' / 'shell.cpp.ulg-backup').is_file()
    assert (concepts / 'registry.cpp.ulg-backup').read_text() == '// original registry\n'
    assert shell.read_text().count('count > 1 &&') == 2
    assert (kit / 'Makefile').read_text().count('# PS5 UNIVERSAL LAUNCHER:') == 1
    assert (concepts / 'concepts.hpp').read_text().count('make_universal_launcher') == 1
    assert (kit / 'assets' / 'paths.conf').exists()


def test_build_script_is_host_only_and_pins_ui_kit():
    script = (ROOT / 'tooling' / 'build_native_ps5.sh').read_text()
    assert 'KIT_COMMIT' in script
    assert 'bbe19054ee2e7ec069254775273aeb8846c4bd3e' in script
    assert 'sce_module/libc.prx' in script and 'eboot.bin' in script
    assert 'make -C "$KIT"' in script
    assert 'ftp://' not in script and 'make deploy' not in script
