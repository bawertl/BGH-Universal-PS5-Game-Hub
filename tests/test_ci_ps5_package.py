"""Check false positives are rejected without possessing any PS5 binaries."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / 'tooling' / 'verify_ps5_package.py'
TITLE = 'PPSA99079'
CONTENT = 'UP9000-PPSA99079_00-UNIVERSALLAUNCH0'
META = {
    'titleId': TITLE,
    'conceptId': '99079',
    'contentId': CONTENT,
    'applicationCategoryType': 0,
    'localizedParameters': {'defaultLanguage': 'de-DE', 'de-DE': {'titleName': 'Universal Game Launcher'}},
}
MAGIC = struct.pack('<I', 0x1D3D154F)
FILES = {
    'eboot.bin': MAGIC + b'ps5 self fixture',
    'sce_module/libc.prx': MAGIC + b'ps5 prx fixture',
    'sce_sys/param.json': json.dumps(META).encode(),
    'sce_sys/icon0.png': b'pngfixture',
    'assets/paths.conf': b'PS2|/data/PCSX2/games',
    'assets/cover_overrides.conf': b'# none',
    'assets/premium_lake_background.png': b'pngfixture',
    'assets/ui/cover_shadow.png': b'RGBA png fixture',
    'assets/ui/cover_frame.png': b'RGBA png fixture',
    'assets/ui/cover_frame_active.png': b'RGBA png fixture',
    'assets/ui/fan_case_header_ps5.png': b'RGBA png fixture',
    'assets/ui/fan_case_header_ps4.png': b'RGBA png fixture',
    'assets/ui/fan_case_header_switch.png': b'RGBA png fixture',
    'assets/ui/serif_atlas.png': b'RGBA png fixture',
}


def make_zip(path, files=None, mode=0o777):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        for name, data in (files or FILES).items():
            info = zipfile.ZipInfo(TITLE + '/' + name)
            info.create_system = 3
            info.external_attr = (0o100000 | mode) << 16
            z.writestr(info, data)


def run(path):
    return subprocess.run([sys.executable, str(VALIDATOR), str(path), TITLE],
                          text=True, capture_output=True)


def test_ps5_package_shape_valid(tmp_path):
    p = tmp_path / 'good.zip'
    make_zip(p)
    result = run(p)
    assert result.returncode == 0, result.stderr
    assert 'NO CONSOLE TEST' in result.stdout


def test_linux_elf_rejected(tmp_path):
    p = tmp_path / 'host.zip'
    fake = dict(FILES)
    fake['eboot.bin'] = b'\x7fELF' + b'linux program'
    make_zip(p, fake)
    result = run(p)
    assert result.returncode != 0
    assert 'Linux ELF rejected' in result.stderr


def test_broken_modes_rejected(tmp_path):
    p = tmp_path / 'modes.zip'
    make_zip(p, mode=0o644)
    assert run(p).returncode != 0


def test_wrong_title_rejected(tmp_path):
    p = tmp_path / 'wrong.zip'
    fake = dict(FILES)
    fake['sce_sys/param.json'] = json.dumps(dict(META, titleId='PPSA99100')).encode()
    make_zip(p, fake)
    assert run(p).returncode != 0


def test_missing_runtime_rejected(tmp_path):
    p = tmp_path / 'missing.zip'
    make_zip(p, {k: v for k, v in FILES.items() if k != 'sce_module/libc.prx'})
    assert run(p).returncode != 0


def test_corrupt_zip_rejected(tmp_path):
    p = tmp_path / 'bad.zip'
    p.write_bytes(b'not a zip')
    assert run(p).returncode != 0


def test_build_script_does_not_recompress():
    text = (ROOT / 'tooling' / 'build_native_ps5.sh').read_text()
    assert 'SOURCE_ZIP="$KIT/dist/$TITLE_ID.zip"' in text
    assert 'zip -q -r' not in '\n'.join(line for line in text.splitlines() if not line.strip().startswith('#'))
    assert 'verify_ps5_package.py' in text
