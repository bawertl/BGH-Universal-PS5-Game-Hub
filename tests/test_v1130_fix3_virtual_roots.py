"""Fix3: PS5 sandbox-safe root hub, FTP fallback, and controller paths."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
RM=(ROOT/'native_ps5/root_manager.cpp').read_text()
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()
def test_virtual_mount_roots_and_ftp_fallback():
    assert 'if(path=="/")' in RM
    assert 'Virtual PS5 storage roots' in RM
    assert '"/data","/data/homebrew","/user/app"' in RM
    assert '"/mnt/usb0","/mnt/usb1","/mnt/usb2","/mnt/usb3"' in RM
    assert 'if(!available && ftp_ready)' in RM
    assert 'out.state=make_root_error("Browse",path,"opendir",errno);' in RM
    assert 'const auto remote=ftp.list(path,max_entries);' in RM
    assert 'out.state={"Browse",path,"ftp_ok",0,"FTP MLSD"};' in RM
def test_virtual_root_selection_and_ftp_prefill():
    assert 'state=="ok" || state=="ftp_ok"' in UI
    assert 'if(chosen=="/" && count>0)' in UI
    assert 'save_emulator_path(chosen);' in UI
    assert 'else add_root_path(chosen);' in UI
    assert 'if(input.is_pressed(Action::touch))' in UI
    assert 'browser_path_=end==0?"/":browser_path_.substr(0,end);' in UI
def test_fix3_branch():
    assert 'branches: [bgh-v1.1.31-fix2]' in WF
    assert 'test "$GITHUB_REF_NAME" = \'bgh-v1.1.31-fix2\'' in WF
