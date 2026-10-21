"""V1.0.8 locale, help, safe settings persistence and UI routing."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
STATE=(ROOT/'native_ps5/state_store.cpp').read_text()
HDR=(ROOT/'native_ps5/state_store.hpp').read_text()
LOC=(ROOT/'native_ps5/ui_localization.hpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()


def test_language_is_saved_without_touching_previous_formats():
    assert 'struct LanguageSettings' in HDR
    assert 'bool english=false;' in HDR
    assert 'return save(path,"ULGL",w,status);' in STATE
    assert 'load(path,"ULGL",bytes,status)' in STATE
    assert 'return save(path,"ULGP",w,status);' in STATE
    assert 'return save(path,"ULGE",w,status);' in STATE
    assert 'kPrimaryLanguageFile' in CPP and 'kFallbackLanguageFile' in CPP
    assert 'fallback_language.revision>primary_language.revision' in CPP
    assert 'if(!primary && !fallback)' in CPP
    assert 'save_language_setting(!language_settings_.english)' in CPP


def test_settings_shortcuts_and_help_scoped_to_settings():
    update=CPP.split('void update_settings(',1)[1].split('void refresh_browser()',1)[0]
    assert 'settings_tab_==1' in update
    assert 'presentation_selected_=std::max(0,presentation_selected_-1)' in update
    assert 'presentation_selected_=std::min(3,presentation_selected_+1)' in update
    assert 'save_emulator_boot_setting(!emulator_boot_settings_.direct_boot)' in update
    assert 'save_language_setting(!language_settings_.english)' in update
    assert 'if(settings_tab_!=3)return;' in update
    assert 'root.enabled=!root.enabled;' in update
    assert 'English' in CPP and 'Deutsch' in CPP and 'Sprache wechseln' in CPP
    assert 'Controller-Hilfe' in CPP and 'Options: Settings' in CPP
    assert 'X: Spielen / Menue' in CPP and 'L2/R2: Ordnerfilter' in CPP
    assert 'draw_settings_system(list)' in CPP


def test_translation_coverage_version_and_hints():
    for key in ['Meine Spiele','Zuletzt gespielt','Darstellung & Verhalten','Spieleordner',
                'Emulator-Startverhalten','Controller-Hilfe','Spielpfad bearbeiten',
                'Ordnerbrowser (nur lesend)']:
        assert f'"{key}"' in LOC
    assert 'const char *hint(' in CPP
    assert 'ulg::tr_hint(label,language_settings_.english)' in CPP
    assert 'ulg::translate_ui(value,language_settings_.english)' in CPP
    assert 'BGH V1.1.8-PLAYTIME-PER-GAME-SAFETY' in CPP
    assert "ui_localization.hpp" in (ROOT/'tooling/install_ui_kit_overlay.py').read_text()
    assert 'release_label="BGH V1.1.8"' in CPP
    assert 'branches: [bgh-v1.1.8, main]' in WF
    assert 'BGH-V1-1-8-PLAYTIME-PPSA99079-TEST' in WF
    assert 'V1_1_8_PPSA99079_PS5_TEST.zip' in WF


def test_language_binary_roundtrip_and_corruption(tmp_path):
    cli=tmp_path/'language-test'
    cmd=['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
         '-Inative_ps5','native_ps5/language_settings_test_cli.cpp',
         'native_ps5/state_store.cpp','-o',str(cli)]
    subprocess.run(cmd,cwd=ROOT,check=True,capture_output=True,text=True,timeout=90)
    result=subprocess.run([str(cli),str(tmp_path)],capture_output=True,text=True,timeout=15)
    assert result.returncode==0,(result.stdout,result.stderr,result.returncode)
    assert 'ULGP/ULGE unchanged' in result.stdout
