#!/usr/bin/env python3
"""Fail closed: validate the genuine PS5 title ZIP produced by ps5-homebrew-ui.

This checks container shape, SELF/FSELF magic, title metadata, asset presence,
upstream-mandated ZIP modes and CRCs. It is NOT a hardware compatibility test.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import struct
import sys
import zipfile

# Upstream kit signs the native title and runtime with these SELF/FSELF magics.
NATIVE_MAGICS = {0x1D3D154F, 0xEEF51454}
REQUIRED = (
    "eboot.bin",
    "sce_module/libc.prx",
    "sce_sys/param.json",
    "sce_sys/icon0.png",
    "assets/paths.conf",
    "assets/cover_overrides.conf",
    "assets/premium_lake_background.png",
    "assets/ui/cover_shadow.png",
    "assets/ui/cover_frame.png",
    "assets/ui/cover_frame_active.png",
    "assets/ui/fan_case_header_ps5.png",
    "assets/ui/fan_case_header_ps4.png",
    "assets/ui/fan_case_header_switch.png",
    "assets/ui/serif_atlas.png",
)
MAX_METADATA = 262144


def verify(path: Path, title_id: str) -> dict[str, object]:
    if not re.fullmatch(r"PPSA\d{5}", title_id):
        raise ValueError("Invalid development title ID")
    if not path.is_file() or path.stat().st_size == 0:
        raise ValueError("Missing/empty PS5 title ZIP")
    prefix = title_id + "/"
    with zipfile.ZipFile(path) as archive:
        infos = archive.infolist()
        names = [item.filename for item in infos]
        if not infos or len(names) != len(set(names)):
            raise ValueError("Empty ZIP or duplicate file paths")
        for info in infos:
            name = info.filename
            parts = PurePosixPath(name).parts
            if not name.startswith(prefix) or not parts or any(x in (".", "..") for x in parts):
                raise ValueError("Unsafe or unexpected ZIP entry: " + name)
            if info.flag_bits & 1:
                raise ValueError("Encrypted ZIP entry: " + name)
            mode = (info.external_attr >> 16) & 0o777
            if mode != 0o777:
                raise ValueError(f"Unsafe PS5 ZIP permissions {mode:o}: {name}")
        if archive.testzip() is not None:
            raise ValueError("ZIP CRC validation failed")
        missing = [name for name in REQUIRED if prefix + name not in names]
        if missing:
            raise ValueError("Missing required PS5 title files: " + ", ".join(missing))
        for name in REQUIRED:
            info = archive.getinfo(prefix + name)
            if info.file_size == 0 or info.is_dir():
                raise ValueError("Empty/invalid PS5 application file: " + name)
        for name in ("eboot.bin", "sce_module/libc.prx"):
            with archive.open(prefix + name) as stream:
                header = stream.read(4)
            if len(header) != 4 or struct.unpack("<I", header)[0] not in NATIVE_MAGICS:
                raise ValueError(name + " is not a recognised native PS5 SELF/FSELF (ordinary Linux ELF rejected)")
        meta_info = archive.getinfo(prefix + "sce_sys/param.json")
        if meta_info.file_size > MAX_METADATA:
            raise ValueError("Unreasonably large PS5 param.json")
        metadata = json.loads(archive.read(prefix + "sce_sys/param.json"))
        if metadata.get("titleId") != title_id or metadata.get("conceptId") != title_id[-5:]:
            raise ValueError("PS5 titleId or conceptId mismatch")
        if not str(metadata.get("contentId", "")).startswith("UP9000-" + title_id + "_00-"):
            raise ValueError("PS5 contentId mismatch")
        if not isinstance(metadata.get("localizedParameters"), dict):
            raise ValueError("PS5 title name metadata missing")
        if metadata.get("applicationCategoryType") != 0:
            raise ValueError("Expected PS5 game-category homebrew metadata")
    with path.open("rb") as artifact:
        digest = hashlib.file_digest(artifact, "sha256").hexdigest()
    return {
        "path": str(path),
        "title_id": title_id,
        "sha256": digest,
        "entries": len(names),
        "bytes": path.stat().st_size,
        "container": "native-PS5-SELF/FSELF-shape",
        "hardware_tested": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zip", type=Path)
    parser.add_argument("title_id")
    args = parser.parse_args()
    try:
        result = verify(args.zip, args.title_id)
    except (OSError, ValueError, zipfile.BadZipFile, KeyError, json.JSONDecodeError) as error:
        print("PS5 PACKAGE REJECTED: " + str(error), file=sys.stderr)
        return 1
    print("PS5 PACKAGE STATIC CHECK PASSED (NO CONSOLE TEST)")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
