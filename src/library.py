"""Read-only filesystem library scanner. Python is a DEVELOPMENT/HOST runner, not a PS5 runtime.

Never modifies game directories and never invokes emulators. JSON cache is written only
at the explicitly configured output location.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import struct
from typing import Any

EXTENSIONS = {
    'PS2': {'.iso', '.chd', '.cso', '.zso'},
    'Switch': {'.nsp', '.xci'},
    'Xbox 360': {'.xex', '.iso'},
    'PS1': {'.cue', '.chd', '.bin'},
    'PSP': {'.iso', '.cso', '.chd'},
}
IMAGE_EXT = ('.png', '.jpg', '.jpeg', '.webp')
ROOT_TYPES = ('PS5/PS4', 'PS5', 'PS4', 'PS2', 'Switch', 'Xbox 360', 'PS1', 'PSP')
SANITIZE_RE = re.compile(r'[_\-.]+')
TITLE_ID_RE = re.compile(r'(PPSA\d{5}|CUSA\d{5}|[A-Z]{4}[-_]?\d{5})', re.I)


def normalize_title(name: str) -> str:
    name = re.sub(r'\.(iso|chd|cso|zso|nsp|xci|xex)$', '', name, flags=re.I)
    name = re.sub(r'\s*[\[(](?:USA|EUR|EUROPE|JAPAN|PAL|NTSC|EN|DE|USA,.*?)[\])]\s*', ' ', name, flags=re.I)
    return ' '.join(SANITIZE_RE.sub(' ', name).split()) or 'Unbenanntes Spiel'


def read_sfo(path: Path) -> dict[str, str]:
    """Read the documented PSF/SFO key/value layout, without external libraries.

    Reject invalid offsets and oversized content; never executes file contents.
    """
    try:
        if path.stat().st_size > 2_000_000:
            return {}
        buf = path.read_bytes()
        if len(buf) < 20 or buf[:4] != b'\x00PSF':
            return {}
        key_table, data_table, count = struct.unpack_from('<III', buf, 8)
        if count > 256 or key_table >= len(buf) or data_table >= len(buf):
            return {}
        found: dict[str, str] = {}
        for index in range(count):
            off = 20 + index * 16
            if off + 16 > len(buf):
                break
            keyoff, fmt, size, capacity, valoff = struct.unpack_from('<HHIII', buf, off)
            start = key_table + keyoff
            end = buf.find(b'\x00', start, min(len(buf), start + 128)) if start < len(buf) else -1
            if end < 0 or size > 32768 or data_table + valoff + size > len(buf):
                continue
            key = buf[start:end].decode('utf8', 'replace')
            if key not in ('TITLE', 'TITLE_ID', 'CONTENT_ID', 'CATEGORY', 'APP_VER'):
                continue
            raw = buf[data_table + valoff:data_table + valoff + size]
            found[key] = raw.split(b'\x00')[0].decode('utf8', 'replace').strip()
        return found
    except (OSError, ValueError, struct.error):
        return {}


def read_param_json(folder: Path) -> dict[str, str]:
    target = folder / 'sce_sys' / 'param.json'
    try:
        if target.stat().st_size > 500_000:
            return {}
        info = json.loads(target.read_text(encoding='utf8'))
        if not isinstance(info, dict):
            return {}
        # Homebrew manifests can differ; treat names/ids as candidates.
        content = info.get('localizedParameters', {})
        local = content.get('en-US') or content.get('de-DE') or next(iter(content.values()), {}) if isinstance(content, dict) else {}
        title = (local.get('titleName') if isinstance(local, dict) else None) or info.get('titleName') or info.get('title')
        return {'TITLE': str(title or ''), 'TITLE_ID': str(info.get('titleId') or ''), 'CONTENT_ID': str(info.get('contentId') or '')}
    except (OSError, ValueError, TypeError, StopIteration):
        return {}


def find_cover(gamepath: Path, folder_game: bool, extra_roots: list[str], game_id: str = '') -> str | None:
    folder = gamepath if folder_game else gamepath.parent
    candidates: list[Path] = []
    for prefix in ('cover', 'folder', 'icon', 'poster', 'boxart'):
        candidates += [folder / (prefix + ext) for ext in IMAGE_EXT]
    stem = gamepath.stem
    candidates += [folder / (stem + ext) for ext in IMAGE_EXT]
    candidates += [folder / 'sce_sys' / 'icon0.png', folder / 'sce_sys' / 'icon0.jpg']
    if game_id:
        for root in extra_roots:
            candidates += [Path(root) / (game_id + ext) for ext in IMAGE_EXT]
    for candidate in candidates:
        try:
            if candidate.is_file() and 0 < candidate.stat().st_size < 20_000_000:
                return str(candidate)
        except OSError:
            continue
    return None


def make_item(platform: str, path: Path, file_format: str, title: str, *, title_id: str = '', cover: str | None = None, metadata: str = 'Ordnername') -> dict[str, Any]:
    absolute = str(path.absolute())
    uid = hashlib.sha256((platform + '\0' + absolute).encode('utf8', 'replace')).hexdigest()[:20]
    return {
        'id': uid, 'title': title, 'platform': platform, 'format': file_format,
        'path': absolute, 'title_id': title_id, 'cover_path': cover,
        'available': True, 'metadata_source': metadata, 'launch_status': 'nicht_verifiziert'
    }


def inspect_native_folder(folder: Path, cover_roots: list[str]) -> dict[str, Any] | None:
    metadata = read_param_json(folder)
    source = 'param.json' if any(metadata.values()) else ''
    if not source:
        sfo = read_sfo(folder / 'sce_sys' / 'param.sfo')
        if not sfo:
            sfo = read_sfo(folder / 'PS4_GAME' / 'PARAM.SFO')
        metadata = sfo
        source = 'param.sfo' if sfo else ''
    name = folder.name
    explicit_id = metadata.get('TITLE_ID', '').upper()
    guessed = TITLE_ID_RE.search(name)
    title_id = explicit_id or (guessed.group(1).upper().replace('_', '-') if guessed else '')
    # Avoid claiming a PS5 or PS4 identification just because the path is named 'games'.
    if title_id.startswith('CUSA'):
        platform = 'PS4'
    elif title_id.startswith('PPSA'):
        platform = 'PS5'
    elif (folder / 'PS4_GAME' / 'PARAM.SFO').is_file():
        platform = 'PS4'
    else:
        platform = 'PS5/PS4'
    has_payload = any((folder / leaf).exists() for leaf in ('sce_sys/param.json', 'sce_sys/param.sfo', 'PS4_GAME/PARAM.SFO', 'eboot.bin', 'app.pkg'))
    if not has_payload:
        return None
    title = metadata.get('TITLE') or normalize_title(name)
    cover = find_cover(folder, True, cover_roots, title_id)
    return make_item(platform, folder, 'installationsordner', title, title_id=title_id, cover=cover, metadata=source or 'Ordnername (unklar)')


def _walk(root: Path, depth_max: int, errors: list[dict[str, str]]):
    """Yield shallow-to-deep paths while avoiding symlink traversal/cycles."""
    stack = [(root, 0)]
    while stack:
        folder, depth = stack.pop()
        try:
            with os.scandir(folder) as scan:
                entries = list(scan)
        except OSError as exc:
            errors.append({'path': str(folder), 'error': type(exc).__name__})
            continue
        files = [e for e in entries if e.is_file(follow_symlinks=False)]
        dirs = [e for e in entries if e.is_dir(follow_symlinks=False) and not e.name.startswith('.')]
        yield folder, files, dirs
        if depth < depth_max:
            stack.extend((Path(entry.path), depth + 1) for entry in reversed(dirs))


def scan(config: dict[str, Any], previous: dict[str, Any] | None = None) -> dict[str, Any]:
    games: dict[str, dict[str, Any]] = {}
    unavailable: list[dict[str, str]] = []
    errors: list[dict[str, str]] = []
    cover_roots = [str(x) for x in config.get('cover_paths', [])]
    depth = min(max(int(config.get('max_depth', 6)), 0), 20)
    roots = [(kind, path) for kind, values in config.get('paths', {}).items() if kind in ROOT_TYPES for path in values]
    roots.extend((row.get('platform', ''), row.get('path', '')) for row in config.get('custom_paths', []) if isinstance(row, dict))
    for platform, path in roots:
        if platform not in ROOT_TYPES or not isinstance(path, str) or not path.startswith('/'):
            continue
        root = Path(path)
        if not root.is_dir() or root.is_symlink():
            unavailable.append({'platform': platform, 'path': path})
            continue
        for folder, files, dirs in _walk(root, depth, errors):
            if platform in ('PS5', 'PS4', 'PS5/PS4'):
                # A title is represented once by its installation folder (not every binary inside it).
                if folder != root:
                    item = inspect_native_folder(folder, cover_roots)
                    if item:
                        if platform in ('PS4', 'PS5') and item['platform'] == 'PS5/PS4':
                            item['platform'] = platform
                            item['id'] = hashlib.sha256((platform + '\0' + item['path']).encode()).hexdigest()[:20]
                        games[item['id']] = item
                        dirs[:] = []
                continue
            allowed = EXTENSIONS.get(platform, set())
            for entry in files:
                ext = Path(entry.name).suffix.lower()
                if ext not in allowed:
                    continue
                fpath = Path(entry.path)
                if platform == 'Xbox 360' and ext == '.xex' and entry.name.lower() != 'default.xex':
                    continue
                if platform == 'PS1' and ext == '.bin' and any(Path(x.name).suffix.lower() == '.cue' for x in files):
                    continue
                if platform == 'Xbox 360' and ext == '.xex':
                    name = normalize_title(folder.name)
                else:
                    name = normalize_title(fpath.name)
                cover = find_cover(fpath, False, cover_roots)
                item = make_item(platform, fpath, ext.lstrip('.').upper(), name, cover=cover)
                games[item['id']] = item
            if platform == 'Xbox 360':
                for entry in files:
                    try:
                        # Xbox 360 STFS packages begin with CON , PIRS or LIVE. Extensionless GOD included.
                        if Path(entry.name).suffix or entry.name.lower() in ('default.xex',):
                            continue
                        if entry.stat(follow_symlinks=False).st_size < 4:
                            continue
                        with open(entry.path, 'rb') as handle:
                            signature = handle.read(4)
                        if signature not in (b'CON ', b'PIRS', b'LIVE'):
                            continue
                        fpath = Path(entry.path)
                        item = make_item('Xbox 360', fpath, 'STFS', normalize_title(folder.name if folder != root else fpath.name), cover=find_cover(fpath, False, cover_roots))
                        games[item['id']] = item
                    except OSError as exc:
                        errors.append({'path': entry.path, 'error': type(exc).__name__})
    # Do not mistake unplugged USB storage for deleted games. Restore previous entries as unavailable.
    for old in (previous or {}).get('games', []):
        if not isinstance(old, dict) or not isinstance(old.get('path'), str) or not isinstance(old.get('id'), str):
            continue
        if old['id'] in games:
            continue
        old_path = os.path.normpath(old['path'])
        if any(old_path == os.path.normpath(row['path']) or old_path.startswith(os.path.normpath(row['path']).rstrip('/') + '/') for row in unavailable):
            copy = dict(old)
            copy['available'] = False
            games[copy['id']] = copy
    return {
        'schema': 2, 'generated_at': datetime.now(timezone.utc).isoformat(),
        'games': sorted(games.values(), key=lambda x: (x['title'].casefold(), x['platform'], x['path'])),
        'unavailable_paths': sorted(unavailable, key=lambda x: x['path']), 'errors': errors,
        'scan_source': 'read_only_host_scanner',
    }


def load_json(path: Path, fallback: dict[str, Any] | None = None) -> dict[str, Any]:
    try:
        with path.open('r', encoding='utf8') as file:
            data = json.load(file)
            return data if isinstance(data, dict) else (fallback or {})
    except (OSError, ValueError):
        return fallback or {}


def scan_to_files(config_file: Path, output: Path, ui_data: Path | None = None) -> dict[str, Any]:
    config = load_json(config_file)
    previous = load_json(output)
    result = scan(config, previous)
    payload = json.dumps(result, ensure_ascii=False, indent=2)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(payload, encoding='utf8')
    if ui_data:
        ui_data.parent.mkdir(parents=True, exist_ok=True)
        ui_data.write_text('window.PS5UL_LIBRARY = ' + payload + ';\n', encoding='utf8')
    return result


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description='PS5 Universal Launcher – schreibgeschützter Dateiscan (Host)')
    parser.add_argument('--config', type=Path, default=root / 'config' / 'paths.json')
    parser.add_argument('--output', type=Path, default=root / 'data' / 'library-cache.json')
    parser.add_argument('--ui-data', type=Path, default=root / 'ui' / 'library-data.js')
    args = parser.parse_args()
    result = scan_to_files(args.config, args.output, args.ui_data)
    print(f"Erkannt: {len(result['games'])} | Offline-Verzeichnisse: {len(result['unavailable_paths'])} | Lesefehler: {len(result['errors'])}")

if __name__ == '__main__':
    main()
