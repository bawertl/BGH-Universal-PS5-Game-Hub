"""V1.0.8: emulator-only launch option without changing proven game starts."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
STATE=(ROOT/'native_ps5/state_store.cpp').read_text()
HDR=(ROOT/'native_ps5/state_store.hpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()


def test_new_menu_mode_scoped_and_recent_safe():
    start=CPP.split('void request_selected_launch(',1)[1].split('void refresh_storage',1)[0]
    assert 'if(!emulator_boot_settings_.direct_boot &&' in start
    branch=start.split('if(!emulator_boot_settings_.direct_boot &&',1)[1].split('launch_in_progress_=true;',1)[0]
    for platform in ('"PS2"', '"Switch"', '"Xbox 360"'):
        assert platform in branch
    assert 'emulator_menu_plan(selected_game.platform)' in branch
    assert 'menu_plan=emulator_menu_plan(selected_game.platform)' in branch
    assert 'ulg::send_launch(menu_plan)' in branch
    assert 'emulator_detection_for(selected_game.platform)' in branch
    assert 'stage_ps5sx2_disc_launch' not in branch
    assert 'record_recent(' not in branch
    assert 'sys::quit()' in branch
    assert 'if(!selected_game.available)' not in branch # Menu-only launch never reads ROM
    # Existing direct route and pre-launch persistence must stay in place.
    assert 'stage_ps5sx2_disc_launch(g.path)' in start
    assert 'make_registered_ps4_launch_plan(g.title_id)' in start
    assert 'make_registered_ps5_launch_plan(g.title_id)' in start
    assert 'record_recent(g.id)' in start
    assert 'void record_recent(' in CPP


def test_settings_tab_toggle_and_legacy_format_unchanged():
    update=CPP.split('void update_settings(',1)[1].split('void refresh_browser()',1)[0]
    assert 'if(settings_tab_==1)' in update
    assert 'if(input.is_pressed(Action::confirm))' in update
    assert 'save_emulator_boot_setting(!emulator_boot_settings_.direct_boot)' in update
    assert 'if(settings_tab_!=3)return;' in update
    assert 'kPrimaryEmulatorSettingsFile' in CPP
    assert 'kFallbackEmulatorSettingsFile' in CPP
    assert 'fallback_boot.revision>primary_boot.revision' in CPP
    assert 'if(!primary && !fallback)' in CPP
    assert 'out=EmulatorBootSettings{direct!=0u,revision};' in STATE
    assert 'return save(path,"ULGE",w,status);' in STATE
    assert 'load(path,"ULGE",bytes,status)' in STATE
    assert 'return save(path,"ULGP",w,status);' in STATE
    assert 'load(path,"ULGP",bytes,status)' in STATE
    assert 'emulator_boot_settings_' in CPP
    assert 'recent_list_' in CPP
    assert 'bool direct_boot=true;' in HDR
    assert 'VERSION' not in STATE or 'ULGP' in STATE


def test_menu_ui_and_version_in_sync():
    panel=CPP.split('void draw_settings_presentation(',1)[1].split('void draw_settings_system(',1)[0]
    assert 'Nur Emulator-Menue' in panel
    assert 'Spiele direkt starten' in panel
    assert 'Emulator-Startverhalten' in panel
    assert 'English' in panel and 'Deutsch' in panel
    assert 'BGH V1.1.30' in CPP
    assert 'release_label="BGH V1.1.9"' in CPP
    assert 'branches: [bgh-v1.1.30-fix1]' in WF
    assert 'BGH-V1-1-30-FIX1-PPSA99079' in WF
    assert 'V1_1_30_PPSA99079_PS5_TEST.zip' in WF


def test_native_state_roundtrip_and_corruption(tmp_path):
    cli=tmp_path/'emulator_settings_test_cli'
    compile_cmd=['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
                 '-Inative_ps5','native_ps5/emulator_settings_test_cli.cpp',
                 'native_ps5/state_store.cpp','-o',str(cli)]
    subprocess.run(compile_cmd,cwd=ROOT,check=True,capture_output=True,text=True,timeout=60)
    result=subprocess.run([str(cli),str(tmp_path)],capture_output=True,text=True,timeout=15)
    assert result.returncode==0,(result.stdout,result.stderr,result.returncode)
    assert 'ULGP-preserved' in result.stdout
