"""Host verification for the exact POSIX code reused in the native PS5 UI."""
from pathlib import Path
import json
import subprocess
import pytest


@pytest.fixture(scope='session')
def scanner():
    root = Path(__file__).resolve().parents[1]
    subprocess.run(['make', '-C', str(root/'native_ps5'), 'CXX=g++'], check=True, capture_output=True)
    return root/'native_ps5'/'scanner-host'


def run(scanner, root, entries, output):
    paths=root/'paths.conf'
    paths.write_text('\n'.join(platform+'|'+str(path) for platform,path in entries), encoding='utf8')
    subprocess.run([str(scanner),'--paths',str(paths),'--output',str(output)],capture_output=True,text=True,check=True)
    return json.loads(output.read_text(encoding='utf8'))


def test_read_only_native_scanner(scanner,tmp_path):
    native=tmp_path/'native'; app=native/'PPSA99998';(app/'sce_sys').mkdir(parents=True)
    (app/'sce_sys'/'param.json').write_text('{"titleId":"PPSA99998"}')
    ps2=tmp_path/'ps2';ps2.mkdir();(ps2/'Silent Hill 2.chd').write_bytes(b'not a full image')
    switch=tmp_path/'switch';switch.mkdir();(switch/'Zelda.xci').write_bytes(b'test')
    xbox=tmp_path/'xbox360';xbox.mkdir();(xbox/'Lost Odyssey').mkdir()
    (xbox/'Lost Odyssey'/'default.xex').write_bytes(b'test')
    (xbox/'Lost Odyssey'/'4D53ABCD').write_bytes(b'CON '+b'\0'*8)
    cache=tmp_path.parent/'test-result-cache.json'
    result=run(scanner,tmp_path,[('PS5/PS4',native),('PS2',ps2),('Switch',switch),('Xbox 360',xbox)],cache)
    assert len(result['games'])==5
    assert {g['platform'] for g in result['games']}=={'PS5','PS2','Switch','Xbox 360'}
    assert all(g['available'] for g in result['games'])
    assert all(g['launch_status']=='nicht_verifiziert' for g in result['games'])
    assert (ps2/'Silent Hill 2.chd').read_bytes()==b'not a full image'


def test_usb_missing_and_symlink_ignore(scanner,tmp_path):
    root=tmp_path/'games';root.mkdir();external=tmp_path/'external';external.mkdir()
    (external/'not_included.xci').write_bytes(b'xx')
    (root/'link').symlink_to(external,target_is_directory=True)
    cache=tmp_path/'cache.json'
    output=run(scanner,tmp_path,[('Switch',root),('PS2',tmp_path/'usb-offline')],cache)
    assert output['games']==[]
    assert len(output['unavailable_paths'])==1
    assert output['unavailable_paths'][0]['status']=='offline'


def test_no_root_scan_or_path_traversal(scanner,tmp_path):
    config=tmp_path/'paths.conf'
    config.write_text('PS2|/\nPS2|/tmp/../etc\nSwitch|'+str(tmp_path/'missing')+'\n')
    out=tmp_path/'result.json'
    subprocess.run([str(scanner),'--paths',str(config),'--output',str(out)],capture_output=True,check=True)
    result=json.loads(out.read_text())
    assert len(result['games'])==0
    assert len(result['errors'])==1 # fehlender, aber gültiger Pfad muss jetzt als Fehler zählen
    assert result['failed_operations']==1 and result['failed_roots']==1
