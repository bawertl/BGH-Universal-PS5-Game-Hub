#!/usr/bin/env python3
"""Validate real pinned stb JPEG/TGA decoder, no PS5 or emulator access."""
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess
import sys
from PIL import Image

exe=Path(sys.argv[1]).resolve()
with TemporaryDirectory(prefix='bgh-cover-decoder-') as d:
    folder=Path(d)
    for mode,format,name,options in [
        ('RGB','JPEG','normal.jpg',{}),
        ('RGB','JPEG','progressive.jpg',{'progressive':True}),
        ('RGBA','TGA','rgba.tga',{}),
        ('RGB','TGA','rgb.tga',{}),
        ('RGBA','TGA','rle.tga',{'compression':'tga_rle'}),
    ]:
        art=folder/name
        Image.new(mode,(91,123),(30,120,245,255) if mode=='RGBA' else (30,120,245)).save(art,format,**options)
        subprocess.run([str(exe),'valid',str(art)],check=True,timeout=10)
    bad=folder/'broken.jpg'
    bad.write_bytes(b'\xff\xd8\xff'+b'broken data'*30)
    subprocess.run([str(exe),'invalid',str(bad)],check=True,timeout=10)
    oversized=folder/'oversized.jpg'
    with oversized.open('wb') as stream: stream.truncate(8*1024*1024+1)
    subprocess.run([str(exe),'invalid',str(oversized)],check=True,timeout=10)
    subprocess.run([str(exe),'invalid',str(folder/'absent.jpg')],check=True,timeout=10)
print('BGH V0.12.5: real stb JPEG/TGA decoding tests passed')
