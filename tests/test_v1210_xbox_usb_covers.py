"""BGH V0.12.10: verified external USB JPG names and strict Xbox-game mapping."""
from __future__ import annotations
import hashlib
import subprocess
from pathlib import Path
from PIL import Image
import pytest

ROOT = Path(__file__).resolve().parents[1]

@pytest.fixture(scope='module')
def tool(tmp_path_factory):
    exe=tmp_path_factory.mktemp('v1210-xbox')/'emulator-import'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
        str(ROOT/'native_ps5/emulator_cover_test_cli.cpp'),
        str(ROOT/'native_ps5/emulator_covers.cpp'),
        str(ROOT/'native_ps5/ftp_readonly.cpp'),'-o',str(exe)],check=True)
    return exe

def make_jpg(path):
    Image.new('RGB',(75,100),(31,100,161)).save(path,'JPEG',quality=88)

def run(tool,iso,title_id,source,cache,game_id):
    # The host harness injects xbox_user_covers=source; PS5 defaults to /mnt/usb1/xbox360/Cover.
    return subprocess.run([str(tool),'import','Xbox 360',str(iso),game_id,title_id,
        str(source),str(cache),'2121',''],capture_output=True,text=True,timeout=8)

@pytest.mark.parametrize('folder,iso_name,title_id,filename',[
    ('Gears of War 2','Gears of War 2 (Europe).iso','4D53082D','Gears of War 2.jpg'),
    ('Lord of the Rings, The - War in the North','Lord of the Rings, The - War in the North.iso','575207EF','Lord of the Rings War in the North.jpg'),
    ('Viva Pinata - Party Animals','Viva Pinata - Party Animals.iso','11111111','Viva Party Animals.jpg'),
])
def test_exact_user_filename_imports_without_touching_source(tool,tmp_path,folder,iso_name,title_id,filename):
    source=tmp_path/'Cover';source.mkdir(exist_ok=True)
    cache=tmp_path/'BGH-CACHE';cache.mkdir(exist_ok=True)
    iso=tmp_path/'xbox360'/folder/iso_name
    iso.parent.mkdir(parents=True)
    art=source/filename
    make_jpg(art)
    before=hashlib.sha256(art.read_bytes()).hexdigest()
    result=run(tool,iso,title_id,source,cache,folder.replace(' ','_'))
    assert result.returncode==0,result.stdout+result.stderr
    assert 'COVER IMPORTIERT' in result.stdout
    assert (cache/(folder.replace(' ','_')+'.jpg')).read_bytes()==art.read_bytes()
    assert hashlib.sha256(art.read_bytes()).hexdigest()==before
    assert not iso.exists(), 'Test must not create or modify game ISOs'

def test_unrelated_game_does_not_receive_another_title_cover(tool,tmp_path):
    source=tmp_path/'Cover';source.mkdir()
    cache=tmp_path/'Cache';cache.mkdir()
    make_jpg(source/'Gears of War 2.jpg')
    iso=tmp_path/'xbox360'/'Unrelated Game'/'Unknown.iso'
    result=run(tool,iso,'12345678',source,cache,'unknown_game')
    assert result.returncode==1,result.stdout
    assert not list(cache.iterdir())

def test_existing_cover_is_never_overwritten(tool,tmp_path):
    source=tmp_path/'Cover';source.mkdir()
    cache=tmp_path/'Cache';cache.mkdir()
    art=source/'Gears of War 2.jpg';make_jpg(art)
    already=tmp_path/'approved.png';already.write_bytes(b'kept')
    iso=tmp_path/'xbox360'/'Gears of War 2'/'Gears of War 2.iso'
    result=subprocess.run([str(tool),'import','Xbox 360',str(iso),'gears2','4D53082D',
        str(source),str(cache),'2121',str(already)],capture_output=True,text=True,timeout=8)
    assert result.returncode==0 and 'BEREITS ZUGEORDNET' in result.stdout
    assert already.read_bytes()==b'kept' and not list(cache.iterdir())

def test_default_cover_folder_and_explicit_names_in_production_code():
    h=(ROOT/'native_ps5/emulator_covers.hpp').read_text()
    cpp=(ROOT/'native_ps5/emulator_covers.cpp').read_text()
    assert '/mnt/usb1/xbox360/Cover' in h
    for filename in ('Gears of War 2.jpg','Lord of the Rings War in the North.jpg','Viva Party Animals.jpg'):
        assert filename in cpp
    assert 'sources_.xbox_covers' in cpp and 'sources_.xbox_installed_covers' in cpp

def test_explicit_usb_cover_has_priority_over_same_disc_cover(tool,tmp_path):
    source=tmp_path/'Cover';source.mkdir()
    cache=tmp_path/'Cache';cache.mkdir()
    iso=tmp_path/'xbox360'/'Gears of War 2'/'Gears of War 2.iso'
    iso.parent.mkdir(parents=True)
    local=iso.parent/'cover.jpg'
    make_jpg(local)
    chosen=source/'Gears of War 2.jpg'
    Image.new('RGB',(75,100),(180,30,60)).save(chosen,'JPEG',quality=85)
    assert local.read_bytes()!=chosen.read_bytes()
    result=run(tool,iso,'4D53082D',source,cache,'gears_priority')
    assert result.returncode==0,result.stdout
    assert (cache/'gears_priority.jpg').read_bytes()==chosen.read_bytes()

def test_user_xbox_cover_via_existing_read_only_ftp_fallback(tool,tmp_path):
    from test_v11_ps2_switch_runtime import serve
    source='/virtual/usb1/xbox360/Cover'
    local=tmp_path/'test.jpg';make_jpg(local)
    payload=local.read_bytes()
    server,thread,handler=serve({source+'/Gears of War 2.jpg':payload})
    try:
        cache=tmp_path/'Cache';cache.mkdir()
        result=subprocess.run([str(tool),'import','Xbox 360',
            '/mnt/usb1/xbox360/Gears of War 2/Gears of War 2.iso',
            'gears-ftp','4D53082D',source,str(cache),str(server.server_address[1]),''],
            capture_output=True,text=True,timeout=20)
    finally:
        server.shutdown();server.server_close();thread.join(timeout=3)
    assert result.returncode==0,result.stdout+result.stderr
    assert (cache/'gears-ftp.jpg').read_bytes()==payload
    assert 'RETR' in handler.commands
    assert set(handler.commands).isdisjoint({'STOR','DELE','MKD','RMD'})
