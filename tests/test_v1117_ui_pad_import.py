"""V1.1.17 UI and Pad import regression; no claim of PS5 runtime validation."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
PAD=(ROOT/'native_ps5/controller_status.hpp').read_text()
OVERLAY=(ROOT/'tooling/install_ui_kit_overlay.py').read_text()
WORKFLOW=(ROOT/'.github/workflows/ps5-build.yml').read_text()


def test_pad_linked_imports_and_primary_ownership():
    assert 'scePadGetHandle(user,type,index)' in PAD
    assert 'scePadOpen(user,type,index,nullptr)' in PAD
    assert 'scePadReadState(handle,sample)' in PAD
    assert 'if(owned)(void)scePadClose(handle);' in PAD
    assert 'dlsym(' not in PAD and 'dlopen(' not in PAD
    assert 'libScePad.so' in OVERLAY and 'libSceUserService.so' in OVERLAY
    assert 'sceUserServiceGetLoginUserIdList' in PAD
    assert 'next_controller_poll_=5.0f' in UI
    assert 'controllers_.observe_primary(input.connected);' in UI
    assert 'state[0]=connected?1:0;' in PAD
    assert 'sample+0x4c' in PAD
    assert '0x54' not in PAD


def test_visual_changes_but_no_game_launch_regressions():
    rail=UI.split('void draw_platform_bar(',1)[1].split('void draw_storage_widgets(',1)[0]
    assert 'const int n=static_cast<int>(sidebar_items_.size());' in rail
    sidebar=UI.split('void rebuild_sidebar()',1)[1].split('bool persist_platform_registry(',1)[0]
    assert 'items.push_back({"Apps"' not in sidebar
    assert 'items.push_back({"Favoriten"' not in sidebar
    assert 'items.push_back({"Zuletzt"' not in sidebar
    assert 'list.gradient_rect(launch_button,15,kActiveTabStart,kActiveTabEnd);' in UI
    assert 'list.gradient_rect(pill,17,kActiveTabStart,kActiveTabEnd);' in rail
    assert 'translated_text(list,fonts.semibold,music_settings_.enabled?"Musik: AN":"Musik: AUS",1580,1057,16' in UI
    assert 'translated_text(list,fonts.semibold,release_label,1720,1057,14' in UI
    assert 'std::floor(cursor+0.5f)' in UI
    assert 'gallery_selection_loaded_after_boot_?' in UI
    assert 'if(!gallery_selection_loaded_after_boot_)' in UI
    assert 'clock_>=2.0f' not in UI
    assert 'statvfs(' not in UI
    assert 'shadow_storage_.diagnostic' in UI
    for api in ('stage_ps5sx2_disc_launch(g.path)',
                'make_registered_ps5_launch_plan(g.title_id)',
                'make_registered_ps4_launch_plan(g.title_id)',
                'ulg::send_launch(plan)'):
        assert api in UI


def test_current_version_and_build():
    assert 'BGH V1.1.26' in UI
    assert 'bgh-v1.1.26-fix1' in WORKFLOW
    assert 'V1_1_26_PPSA99079_PS5_TEST.zip' in WORKFLOW
    assert 'APP_IMPORT_STUBS' in OVERLAY


def test_mock_multi_controller(tmp_path):
    exe=tmp_path/'pad-import'
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror',
        '-DBGH_TARGET_PS5=1','-I'+str(ROOT/'native_ps5'),
        str(ROOT/'tests/controller_direct_v1117_harness.cpp'),
        '-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
