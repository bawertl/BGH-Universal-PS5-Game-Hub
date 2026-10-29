"""V1.0.5 release polish: premium five-cover fan, refined labels and status clock."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
LIB=(ROOT/'native_ps5/library_core.cpp').read_text()
FTP=(ROOT/'native_ps5/ftp_scan.cpp').read_text()
WORKFLOW=(ROOT/'.github/workflows/ps5-build.yml').read_text()
BUILD=(ROOT/'tooling/build_native_ps5.sh').read_text()

def test_release_branding_and_branch():
    assert 'BGH V1.1.32' in CPP
    assert 'bgh-v1.1.32' in WORKFLOW
    assert 'BGH-V1-1-32-PPSA99079' in WORKFLOW
    assert 'BGH_BawertLs_Game_Hub_V1_1_32_PPSA99079_PS5_TEST.zip' in WORKFLOW
    assert 'V1_1_32_${TITLE_ID}_PS5_TEST.zip' in BUILD

def test_seven_cover_fan_and_clock():
    assert 'std::array<Candidate,7> candidates{};' in CPP
    assert 'for(int offset=-3;offset<=3;++offset)' in CPP
    assert 'translated_text(list,fonts.semibold,current_time_label()' in CPP
    assert 'display_platform_name(g.platform)' in CPP
    assert 'ui::Button::cross,hint(menu_mode?"Menue":"Spielen")' in CPP

def test_title_cleanup_for_imported_game_names():
    assert 'removable_title_tag' in LIB
    assert 'removable_title_tag' in FTP
    assert '0100DCA0064A6000' not in LIB
