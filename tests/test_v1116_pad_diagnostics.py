"""Passive PS5 multi-controller diagnostics without claiming physical hardware support."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
PAD=(ROOT/'native_ps5/controller_status.hpp').read_text()
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()

def test_diagnostic_observes_existing_calls_only():
    assert 'last_initial_result=sceUserServiceGetInitialUser(&initial_user);' in PAD
    assert 'last_login_result=sceUserServiceGetLoginUserIdList(users.data());' in PAD
    assert 'last_login_users=users;' in PAD
    assert '++poll_count;' in PAD
    assert 'read_pad(initial_user,0,index,index)' in PAD
    assert 'controllers_.diagnostic_users()' in UI
    assert 'controllers_.diagnostic_pad(1)' in UI
    assert 'controllers_.diagnostic_pad(2)' in UI
    assert 'controllers_.diagnostic_pad(3)' in UI
    assert 'controllers_.observe_primary(input.connected);' in UI
    assert 'next_controller_poll_=clock_+1.0f;' in UI
    assert 'BGH V1.1.24' in UI
    assert 'bgh-v1.1.24-gallery-fix' in WF
    assert 'statvfs(' not in UI

def test_existing_auxiliary_pad_mock(tmp_path):
    exe=tmp_path/'monitor'
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror',
        '-DBGH_TARGET_PS5=1','-I'+str(ROOT/'native_ps5'),str(ROOT/'tests/controller_direct_v1117_harness.cpp'),
        '-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
