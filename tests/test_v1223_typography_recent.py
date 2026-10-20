"""BGH 0.12.26 asset, history and safe-layout guardrails."""
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'native_ps5/universal_launcher.cpp').read_text()


def test_serif_atlas_exists_and_is_loaded_and_installed():
    with Image.open(ROOT/'assets/ui/serif_atlas.png') as art:
        assert art.size == (2048,1024)
        assert art.mode == 'RGBA'
        assert art.getchannel('A').getextrema()[0] == 0
    assert (ROOT/'native_ps5/serif_atlas_metrics.hpp').exists()
    assert 'serif_atlas_metrics.hpp' in (ROOT/'tooling/install_ui_kit_overlay.py').read_text()
    assert 'serif_atlas.png' in CPP
    assert 'serif_text(list,shorten(g.title,24)' in CPP
    assert 'serif_text(list,"BawertL\'s Game Hub"' in CPP


def test_recent_history_dual_storage_and_no_game_write():
    assert 'recent_v123.bin' in CPP
    assert 'kPrimaryPrefsFile' in CPP and 'kFallbackPrefsFile' in CPP
    assert 'save_location(kPrimaryPrefsFile,kPrimaryRecentBackupFile' in CPP
    assert 'save_location(kFallbackPrefsFile,kFallbackRecentBackupFile' in CPP
    assert 'recent_matches(' in CPP
    assert 'game_in_recent(g)' in CPP
    assert 'if(recent_save_error_)' in CPP
    assert 'record_recent(g.id)' in CPP
    assert 'stage_ps5sx2_disc_launch(g.path)' in CPP


def test_safe_fan_and_recent_geometry():
    assert 'kFanCentreX = (kGameListRight + (kScreenRight-kFanRightMargin))*0.5f' in CPP
    assert 'depth-1.0f)*194.0f' in CPP
    assert 'const Rect card{x,872,363,108}' in CPP
    assert 'start_layout.cy=launch_button.y+launch_button.h*0.5f' in CPP
    center=(704+(1920-54))/2
    distance=222+2.2*100
    half_w=297*.52/2
    assert center-distance-half_w > 704


def test_workflow_and_asset_version_consistent():
    wf=(ROOT/'.github/workflows/ps5-build.yml').read_text()
    sh=(ROOT/'tooling/build_native_ps5.sh').read_text()
    assert 'bgh-v1.1.5' in wf
    assert 'BGH V1.1.5-CONTAINER-COVER-FIX' in wf
    assert 'BGH V1.1.5-CONTAINER-COVER-FIX' in CPP
    assert 'V1_1_5_PPSA99079_PS5_TEST.zip' in wf
    assert 'V1_1_5_' in sh
    assert 'serif_atlas.png' in wf
