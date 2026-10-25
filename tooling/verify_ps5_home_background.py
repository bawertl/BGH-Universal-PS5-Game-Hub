#!/usr/bin/env python3
"""Verify PS5 BGH selection/launch BC7 DDS backgrounds (DX10, 4K, single mip).

No image conversion happens at packaging time; both precompressed source assets
are part of the checkout to avoid nondeterministic SDK builds.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

WIDTH, HEIGHT = 3840, 2160
HEADER_SIZE = 148
DDS_LENGTH = HEADER_SIZE + WIDTH * HEIGHT


def verify_dds_bytes(data: bytes) -> None:
    if len(data) != DDS_LENGTH:
        raise ValueError(f"Expected {DDS_LENGTH} bytes, got {len(data)}")
    if data[:4] != b'DDS ':
        raise ValueError('Missing DDS file magic')
    get32 = lambda offset: struct.unpack_from('<I', data, offset)[0]
    required = {
        4:124,       # Standard DDS_HEADER
        8:0x81007,   # Caps, height, width, pixel format, linear size
        12:HEIGHT,
        16:WIDTH,
        20:WIDTH * HEIGHT,  # BC7 one byte per input pixel
        28:0,       # no mipmaps
        76:32,      # DDS_PIXELFORMAT size
        80:4,       # FOURCC
        108:0x1000, # DDSCAPS_TEXTURE
        128:98,     # DXGI_FORMAT_BC7_UNORM
        132:3,      # D3D10_RESOURCE_DIMENSION_TEXTURE2D
        136:0,      # no misc flags
        140:1,      # one texture in array
        144:0,
    }
    for offset, expected in required.items():
        if get32(offset) != expected:
            raise ValueError(f'Invalid BC7 DDS header at offset {offset}: {get32(offset)} != {expected}')
    if data[84:88] != b'DX10':
        raise ValueError('PS5 DDS needs DX10 FourCC')
    # This project uses deterministic BC7 mode 6 blocks (LSB unary mode marker).
    if any(data[i] & 0x7F != 0x40 for i in range(HEADER_SIZE, len(data), 16)):
        raise ValueError('BC7 payload contains invalid/unexpected block modes')


def verify_paths(first: Path, second: Path) -> None:
    a=first.read_bytes(); b=second.read_bytes()
    verify_dds_bytes(a); verify_dds_bytes(b)
    if a != b:
        raise ValueError('BGH selection and launch background mismatch')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('Usage: verify_ps5_home_background.py pic0.dds pic1.dds')
    try:
        verify_paths(Path(sys.argv[1]),Path(sys.argv[2]))
    except (ValueError,OSError) as e:
        raise SystemExit('ERROR: BGH PS5 home art validation failed: ' + str(e)) from e
    print('OK: BGH 3840x2160 BC7 DX10 pic0/pic1 validated')
