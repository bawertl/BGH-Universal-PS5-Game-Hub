"""V1.1.14 ShadowMount API contracts; no access to a real PS5 here."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / 'native_ps5/universal_launcher.cpp').read_text()
WF = (ROOT / '.github/workflows/ps5-build.yml').read_text()


def test_storage_client_validates_json_http_and_deduplicates_mounts(tmp_path):
    binary = tmp_path / 'shadowmount-storage-tests'
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Wpedantic','-Werror',
                    '-I'+str(ROOT/'native_ps5'),
                    str(ROOT/'tests/shadowmount_storage_test.cpp'), '-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)


def test_storage_integration_does_not_touch_launch_services():
    client = (ROOT/'native_ps5/storage_shadowmount.hpp').read_text()
    assert 'POST /api/v1/storage HTTP/1.0' in client
    assert '10101' in client and 'INADDR_LOOPBACK' in client
    assert 'statvfs(' not in client
    assert 'api/v1/games' not in client
    assert 'next_shadow_poll_=clock_+60.0f;' in UI
    assert 'next_shadow_poll_=8.0f' in UI
    render = UI.split('void draw_storage_widgets(',1)[1].split('void draw_detail(',1)[0]
    assert 'fetch_storage(' not in render and '::stat(' not in render
    assert 'shadow_storage_.diagnostic' in UI
    assert 'BGH V1.1.31' in UI
    assert 'branches: [bgh-v1.1.31-fix1]' in WF
    for hook in ('make_registered_ps5_launch_plan(g.title_id)',
                 'make_registered_ps4_launch_plan(g.title_id)',
                 'stage_ps5sx2_disc_launch(g.path)', 'ulg::send_launch(plan)'):
        assert hook in UI


def test_native_gui_cpp_syntax():
    subprocess.run(['bash',str(ROOT/'tooling/test_native_gui_syntax.sh')], check=True)
