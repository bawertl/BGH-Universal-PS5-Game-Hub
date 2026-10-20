import json
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from library import scan, read_sfo, normalize_title, scan_to_files


def settings(base: Path):
    return {'max_depth': 5, 'paths': {
        'PS5/PS4': [str(base/'native')],
        'PS2': [str(base/'ps2')],
        'Switch': [str(base/'switch')],
        'Xbox 360': [str(base/'360')],
    },'cover_paths':[]}


def test_formats_and_metadata(tmp_path):
    for f in ('ps2/Okami (Europe).ISO','ps2/Shadow of the Colossus.chd','switch/Zelda.nsp','switch/Mario.xci','360/Lost Odyssey/default.xex','360/Game 2/halo.iso'):
        p=tmp_path/f;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'abc')
    p=tmp_path/'native'/'PPSA99999';(p/'sce_sys').mkdir(parents=True)
    (p/'sce_sys'/'param.json').write_text(json.dumps({'titleId':'PPSA99999','localizedParameters':{'de-DE':{'titleName':'Mein PS5 Spiel'}}}),encoding='utf8')
    (tmp_path/'ps2'/'cover.jpg').write_bytes(b'jpeg')
    result=scan(settings(tmp_path))
    assert len(result['games'])==7, result['games']
    assert {(g['platform'],g['format']) for g in result['games']} >= {('PS2','ISO'),('PS2','CHD'),('Switch','NSP'),('Xbox 360','XEX'),('PS5','installationsordner')}
    assert next(g for g in result['games'] if g['platform']=='PS5')['title']=='Mein PS5 Spiel'
    assert next(g for g in result['games'] if 'Lost Odyssey' in g['title'])['path'].endswith('/default.xex')
    assert all(g['launch_status']=='nicht_verifiziert' for g in result['games'])


def test_unplugged_storage_remains_offline(tmp_path):
    root=tmp_path/'usb1';root.mkdir();(root/'Game.xci').write_bytes(b'x')
    config={'paths':{'Switch':[str(root)]}}
    first=scan(config);assert len(first['games'])==1
    (root/'Game.xci').unlink();root.rmdir()
    second=scan(config,first)
    assert len(second['games'])==1
    assert not second['games'][0]['available']
    assert second['unavailable_paths'][0]['path']==str(root)


def test_overlap_dedup(tmp_path):
    root=tmp_path/'roms';root.mkdir();(root/'Zelda.xci').write_bytes(b'x')
    result=scan({'paths':{'Switch':[str(root),str(root)]}})
    assert len(result['games'])==1


def test_other_extension_and_symlinks_not_followed(tmp_path):
    root=tmp_path/'ps2';root.mkdir()
    (root/'test.txt').write_bytes(b'x')
    hidden=tmp_path/'elsewhere';hidden.mkdir();(hidden/'outside.iso').write_bytes(b'x')
    (root/'link').symlink_to(hidden,target_is_directory=True)
    result=scan({'paths':{'PS2':[str(root)]}})
    assert result['games']==[]


def test_stfs_signature(tmp_path):
    root=tmp_path/'360';root.mkdir();folder=root/'Halo';folder.mkdir()
    (folder/'4D5307E6').write_bytes(b'CON '+b'\0'*32)
    (folder/'NotSTFS').write_bytes(b'NOPE')
    result=scan({'paths':{'Xbox 360':[str(root)]}})
    assert len(result['games'])==1
    assert result['games'][0]['format']=='STFS'


def test_no_game_folder_writes(tmp_path):
    root=tmp_path/'switch';root.mkdir();(root/'Game.xci').write_bytes(b'rom')
    config=tmp_path/'config.json';config.write_text(json.dumps({'paths':{'Switch':[str(root)]}}))
    output=tmp_path/'cache'/'library.json';web=tmp_path/'web'/'library-data.js'
    result=scan_to_files(config,output,web)
    assert len(result['games'])==1
    assert sorted(p.name for p in root.iterdir()) == ['Game.xci']
    assert json.loads(output.read_text())['schema']==2
    assert web.read_text().startswith('window.PS5UL_LIBRARY = ')


def test_invalid_sfo_is_safe(tmp_path):
    file=tmp_path/'param.sfo';file.write_bytes(b'broken')
    assert read_sfo(file)=={}


def test_title_cleaning():
    assert normalize_title('Grand_Theft_Auto_(USA).ISO')=='Grand Theft Auto'
