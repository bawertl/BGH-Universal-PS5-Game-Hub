"""BGH V0.7: metadata-first platform labels and cover priority (no console needed)."""
import json
import subprocess
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native_ps5'


def catalogue(tmp_path, entries):
    subprocess.run(['make', '-C', str(NATIVE), 'CXX=g++', 'scanner-host'], capture_output=True, check=True)
    conf = tmp_path / 'config.txt'
    conf.write_text('\n'.join(f'{platform}|{path}' for platform, path in entries))
    out = tmp_path / 'out.json'
    subprocess.run([str(NATIVE / 'scanner-host'), '--paths', str(conf), '--output', str(out)], check=True)
    return json.loads(out.read_text())['games']


def sfo(title, title_id):
    keys = b'TITLE\0TITLE_ID\0'
    values = title.encode() + b'\0' + title_id.encode() + b'\0'
    pos_id = len(title) + 1
    entries = b''.join((struct.pack('<HHIII', 0, 0x204, len(title) + 1, len(title) + 1, 0),
                        struct.pack('<HHIII', 6, 0x204, len(title_id) + 1, len(title_id) + 1, pos_id)))
    return (b'\0PSF' + struct.pack('<IIII', 0x101, 20 + len(entries), 20 + len(entries) + len(keys), 2)
            + entries + keys + values)


def test_real_ps5_name_from_param_json(tmp_path):
    root = tmp_path / 'games'
    meta = root / 'PPSA12345' / 'sce_sys'
    meta.mkdir(parents=True)
    (meta / 'param.json').write_text(json.dumps({
        'titleId': 'PPSA12345',
        'localizedParameters': {'de-DE': {'titleName': 'Der Richtige Spielname'}}
    }))
    data = catalogue(tmp_path, [('PS5/PS4', root)])
    assert len(data) == 1
    assert (data[0]['title'], data[0]['platform']) == ('Der Richtige Spielname', 'PS5')


def test_ps4_is_detected_by_sfo_even_without_cusa_folder(tmp_path):
    root = tmp_path / 'homebrew'
    meta = root / 'my_custom_folder' / 'sce_sys'
    meta.mkdir(parents=True)
    (meta / 'param.sfo').write_bytes(sfo('Spiel Vier', 'CUSA12345'))
    data = catalogue(tmp_path, [('PS5/PS4', root)])
    assert (data[0]['title'], data[0]['platform'], data[0]['title_id']) == ('Spiel Vier', 'PS4', 'CUSA12345')


def test_known_homebrew_not_misclassified_as_ps5(tmp_path):
    root = tmp_path / 'games'
    for ident in ('PPSA99203', 'PPSA99008', 'PPSA50011', 'PPSA99079'):
        meta = root / ident / 'sce_sys'
        meta.mkdir(parents=True)
        (meta / 'param.json').write_text('{"titleId":"' + ident + '","titleName":"Application"}')
    data = catalogue(tmp_path, [('PS5/PS4', root)])
    assert len(data) == 4
    assert {g['platform'] for g in data} == {'Homebrew'}
    assert {g['title'] for g in data} == {'PS5SX2', 'ProsperoEden', 'PS5X360', "BGH - BawertL's Game Hub"}


def test_unknown_native_app_not_called_ps5(tmp_path):
    root = tmp_path / 'homebrew'
    app = root / 'unknown_application'
    app.mkdir(parents=True)
    (app / 'eboot.bin').write_bytes(b'dummy')
    data = catalogue(tmp_path, [('PS5/PS4', root)])
    assert len(data) == 1 and data[0]['platform'] == 'Unbekannt'


def test_png_boxart_preferred_over_ps5_app_icon(tmp_path):
    root = tmp_path / 'games'
    app = root / 'PPSA12345'
    meta = app / 'sce_sys'
    meta.mkdir(parents=True)
    (meta / 'param.json').write_text('{"titleId":"PPSA12345","titleName":"Game"}')
    (meta / 'icon0.png').write_bytes(b'icon')
    (app / 'cover.png').write_bytes(b'cover')
    data = catalogue(tmp_path, [('PS5/PS4', root)])
    assert data[0]['cover_path'] == str(app / 'cover.png')


def test_unrecognised_jpg_not_reported_as_supported_cover(tmp_path):
    root = tmp_path / 'games'
    app = root / 'PPSA12345'
    meta = app / 'sce_sys'
    meta.mkdir(parents=True)
    (meta / 'param.json').write_text('{"titleId":"PPSA12345","titleName":"Game"}')
    (app / 'cover.jpg').write_bytes(b'fake jpg')
    data = catalogue(tmp_path, [('PS5/PS4', root)])
    assert data[0]['cover_path'] == ''


def test_user_title_override_preserves_game_folder(tmp_path):
    game = tmp_path / 'PPSA12345'
    (game / 'sce_sys').mkdir(parents=True)
    (game / 'sce_sys' / 'param.json').write_text('{"titleId":"PPSA12345"}')
    override = tmp_path / 'titles.conf'
    override.write_text('PPSA12345|Vom Nutzer bestaetigter Titel\n')
    program = tmp_path / 'cli.cpp'
    program.write_text('''#include "library_core.hpp"
#include <iostream>
int main(int argc, char **argv) {
    if(argc!=3)return 3;
    auto games=ulg::scan({{"PS5/PS4", argv[1], true}});
    ulg::apply_title_overrides(games,argv[2]);
    if(games.games.size()!=1)return 4;
    std::cout << games.games.front().title;
    return 0;
}
''')
    exe = tmp_path / 'cli'
    subprocess.run(['g++', '-std=c++20', '-I', str(NATIVE), str(program), str(NATIVE / 'library_core.cpp'), '-o', str(exe)], check=True)
    result = subprocess.run([str(exe), str(tmp_path), str(override)], capture_output=True, text=True, check=True)
    assert result.stdout == 'Vom Nutzer bestaetigter Titel'
    assert (game / 'sce_sys' / 'param.json').read_text() == '{"titleId":"PPSA12345"}'
