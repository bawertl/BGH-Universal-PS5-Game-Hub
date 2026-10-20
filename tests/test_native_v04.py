"""Offline tests of actual C++ V0.4 modules; no PS5, network or jailbreak needed."""
from pathlib import Path
import binascii
import json
import struct
import subprocess
import zlib
import pytest

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native_ps5'

@pytest.fixture(scope='module', autouse=True)
def build():
    subprocess.run(['make', '-C', str(NATIVE), 'CXX=g++'], check=True, capture_output=True)

def encode_png(path, width, height, color=6):
    def chunk(name, data):
        return struct.pack('>I', len(data)) + name + data + struct.pack('>I', binascii.crc32(name+data)&0xffffffff)
    channels={6:4,2:3,0:1}[color]
    scan=b''.join(b'\x00'+bytes([(x+y)&255 for x in range(width) for _ in range(channels)]) for y in range(height))
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB',width,height,8,color,0,0,0))
    data += chunk(b'IDAT',zlib.compress(scan)) + chunk(b'IEND',b'')
    path.write_bytes(data)

def test_native_png_valid(tmp_path):
    path=tmp_path/'art.png';encode_png(path,8,16)
    result=subprocess.run([str(NATIVE/'cover-test-cli'),str(path)],capture_output=True,text=True)
    assert result.returncode==0, result.stderr
    assert '8x16' in result.stdout

def test_png_rejects_corrupt_and_excessively_large(tmp_path):
    path=tmp_path/'art.png';encode_png(path,8,16)
    bad=bytearray(path.read_bytes());bad[-1]^=1;path.write_bytes(bad)
    assert subprocess.run([str(NATIVE/'cover-test-cli'),str(path)],capture_output=True).returncode!=0
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + b'\x00'*400)
    assert subprocess.run([str(NATIVE/'cover-test-cli'),str(path)],capture_output=True).returncode!=0

def test_native_local_cover_and_cache_round_trip(tmp_path):
    games=tmp_path/'PS2';games.mkdir();file=games/'Burnout 3.iso';file.write_bytes(b'x')
    encode_png(games/'cover.png',2,2)
    config=tmp_path/'paths.conf';config.write_text('PS2|'+str(games)+'\n')
    result=tmp_path/'scan.json'
    subprocess.run([str(NATIVE/'scanner-host'),'--paths',str(config),'--output',str(result)],check=True)
    game=json.loads(result.read_text())['games'][0]
    assert game['cover_path']==str(games/'cover.png')
    cache=tmp_path/'library.bin';prefs=tmp_path/'prefs.bin'
    subprocess.run([str(NATIVE/'state-test-cli'),str(config),str(cache),str(prefs)],check=True)
    assert cache.read_bytes().startswith(b'ULG6')
    assert prefs.read_bytes().startswith(b'ULGP')
    old=bytearray(cache.read_bytes());old[10]^=0x10;cache.write_bytes(old)
    # The native reader's checksum rejection is checked by a tiny integration harness.
    src=tmp_path/'read_corrupt.cpp'
    src.write_text('#include "state_store.hpp"\nint main(int argc,char**argv){ulg::Catalogue c;return ulg::read_catalogue_cache(argv[1],c)?1:0;}')
    exe=tmp_path/'read_corrupt'
    subprocess.run(['g++','-std=c++20','-I',str(NATIVE),str(src),str(NATIVE/'state_store.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(cache)],check=True)

def test_merge_preserves_offline_game_without_inventing_present(tmp_path):
    game_root=tmp_path/'usb';game_root.mkdir();(game_root/'Game.xci').write_bytes(b'x')
    src=tmp_path/'merge_test.cpp'
    src.write_text(r'''
#include "library_core.hpp"
#include <cstdio>
int main(int argc,char**argv){
  const std::string dir=argv[1];
  ulg::Catalogue old=ulg::scan({{"Switch",dir}});
  if(old.games.size()!=1)return 1;
  if(std::rename(dir.c_str(),(dir+"-unplugged").c_str())!=0)return 2;
  ulg::Catalogue next=ulg::merge_unavailable(ulg::scan({{"Switch",dir}}),old);
  if(next.games.size()!=1 || next.games[0].available || next.roots[0].status!="offline")return 3;
  return 0;
}''')
    exe=tmp_path/'merge_test'
    subprocess.run(['g++','-std=c++20','-I',str(NATIVE),str(src),str(NATIVE/'library_core.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(game_root)],check=True)

def test_native_gui_syntax_compiles():
    subprocess.run(['bash',str(ROOT/'tooling'/'test_native_gui_syntax.sh')],check=True)

def test_ps5_param_metadata_and_manual_cover_override(tmp_path):
    native=tmp_path/'native';(native/'package'/'sce_sys').mkdir(parents=True)
    (native/'package'/'sce_sys'/'param.json').write_text(json.dumps({'titleId':'PPSA12345','localizedParameters':{'de-DE':{'titleName':'Mein tolles Spiel'}}}))
    ps2=tmp_path/'ps2';ps2.mkdir(); (ps2/'Abenteuer.iso').write_bytes(b'x')
    image=tmp_path/'custom.png';encode_png(image,2,3)
    override=tmp_path/'override.conf';override.write_text(str(ps2/'Abenteuer.iso')+'|'+str(image))
    config=tmp_path/'paths.conf';config.write_text(f'PS5/PS4|{native}\nPS2|{ps2}\n')
    out=tmp_path/'result.json'
    subprocess.run([str(NATIVE/'scanner-host'),'--paths',str(config),'--covers',str(override),'--output',str(out)],check=True)
    games=json.loads(out.read_text())['games']
    assert any(g['title']=='Mein tolles Spiel' and g['title_id']=='PPSA12345' and g['platform']=='PS5' for g in games)
    assert any(g['cover_path']==str(image) and g['platform']=='PS2' for g in games)
