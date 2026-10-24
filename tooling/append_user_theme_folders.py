#!/usr/bin/env python3
"""Add only empty FTP image folders to the already signed native PS5 title ZIP.

Leaves every existing upstream-produced ZIP entry's compressed data unchanged.
Copies no user pictures and does not create/move/overwrite anything on a PS5.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import stat
import zipfile


def append_folders(package: Path, title_id: str, source: Path) -> None:
    if title_id != "PPSA99079":
        raise ValueError("Unexpected BGH PS5 title ID")
    required = ["user_themes/", "user_themes/backgrounds/",
                "user_themes/logos/"]
    for sub in ("backgrounds", "logos"):
        if not (source / sub / ".keep").is_file():
            raise ValueError("Missing source gallery folder: " + sub)
        required.append("user_themes/" + sub + "/.keep")
    with zipfile.ZipFile(package, 'a', allowZip64=True) as archive:
        existing = set(archive.namelist())
        for relative in required:
            name = title_id + "/" + relative
            if name in existing:
                # Upstream may eventually package these directories itself.
                continue
            directory = relative.endswith('/')
            entry = zipfile.ZipInfo(name)
            entry.create_system = 3
            entry.external_attr = ((stat.S_IFDIR if directory else stat.S_IFREG) | 0o777) << 16
            entry.compress_type = zipfile.ZIP_STORED
            archive.writestr(entry, b'')
    with zipfile.ZipFile(package) as archive:
        if archive.testzip() is not None:
            raise ValueError("Corrupted native package after folder append")
        for relative in required:
            entry = archive.getinfo(title_id + '/' + relative)
            if (entry.external_attr >> 16) & 0o777 != 0o777:
                raise ValueError("Incorrect PS5 mode: " + relative)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('title_id')
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    append_folders(args.package, args.title_id, args.source)
