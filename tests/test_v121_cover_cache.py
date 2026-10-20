"""BGH V0.12.1: reproduce Work's missing/private-cache and corrupt-cache defects."""
from pathlib import Path
import subprocess
from PIL import Image
from test_v08_emulator_covers import fnv1a

ROOT=Path(__file__).resolve().parents[1]

def tool(tmp_path):
    binary=tmp_path/'import'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
        str(ROOT/'native_ps5/emulator_cover_test_cli.cpp'),
        str(ROOT/'native_ps5/emulator_covers.cpp'),
        str(ROOT/'native_ps5/ftp_readonly.cpp'),'-o',str(binary)],check=True)
    return binary

def run(executable,rom,source,cache):
    return subprocess.run([str(executable),'import','Switch',str(rom),'switch-id','',str(source),str(cache),'1',''],
                          capture_output=True,text=True,timeout=10)

def make_tga(path):
    Image.new('RGBA',(96,128),(54,211,130,255)).save(path,'TGA')

def test_missing_bgh_cache_directory_has_precise_error(tmp_path):
    binary=tool(tmp_path)
    rom=Path('/mnt/usb1/switch/good.nsp')
    source=tmp_path/'eden';source.mkdir()
    cover=source/(format(fnv1a(rom.name),'08x')+'.tga');make_tga(cover)
    original=cover.read_bytes()
    result=run(binary,rom,source,tmp_path/'MISSING-BGH-CACHE')
    assert result.returncode==1
    assert 'BGH-CACHE-ORDNER NICHT VERFUEGBAR' in result.stdout
    assert 'DEKODIERBAR' not in result.stdout
    assert cover.read_bytes()==original


def test_corrupt_private_cache_is_replaced_by_valid_original(tmp_path):
    binary=tool(tmp_path)
    rom=Path('/mnt/usb1/switch/validgame.nsp')
    source=tmp_path/'eden';source.mkdir()
    cover=source/(format(fnv1a(rom.name),'08x')+'.tga');make_tga(cover)
    dest=tmp_path/'BGH-cache';dest.mkdir()
    target=dest/'switch-id.tga'
    # TGA header is valid but pixel data is truncated.
    target.write_bytes(cover.read_bytes()[:40])
    result=run(binary,rom,source,dest)
    assert result.returncode==0,result.stdout+result.stderr
    assert target.read_bytes()==cover.read_bytes()
    assert not any('.tmp.' in item.name for item in dest.iterdir())


def test_corrupt_origin_does_not_enter_private_cache(tmp_path):
    binary=tool(tmp_path)
    rom=Path('/mnt/usb1/switch/corrupt.nsp')
    source=tmp_path/'eden';source.mkdir()
    cover=source/(format(fnv1a(rom.name),'08x')+'.tga');make_tga(cover)
    cover.write_bytes(cover.read_bytes()[:50])
    dest=tmp_path/'cache';dest.mkdir()
    result=run(binary,rom,source,dest)
    assert result.returncode==1
    assert 'BILD-DEKODIERUNG' in result.stdout
    assert not list(dest.iterdir())
