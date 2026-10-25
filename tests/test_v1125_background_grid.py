"""V1.1.25: nine true bundled backgrounds; visual selector is offline and explicit-save."""
from pathlib import Path
import re
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
HEADER=(ROOT/'native_ps5/bgh_theme_gallery.hpp').read_text()
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()
BUILD=(ROOT/'tooling/build_native_ps5.sh').read_text()
BASE=ROOT/'assets/themes/backgrounds'
IDS=('lake','showroom','mountain_crown','gaming_hall','neon_corridor',
     'metal_blue','neon_wall','orbital_horizon','blue_hall')


def test_every_background_is_embedded_and_all_previews_match():
    block=HEADER.split('if (kind == Kind::background) return {',1)[1].split('};',1)[0]
    items=re.findall(r'\{"([^"]+)", "([^"]+)", false, (?:true|false)\}',block)
    assert tuple(i for i,_ in items)==IDS
    for id_,path in items:
        assert path.startswith('/app0/assets/')
        source=ROOT/path.removeprefix('/app0/')
        assert source.is_file() and source.stat().st_size>100000
        with Image.open(source) as original:
            assert original.width >= 1200 and original.height >= 675
            assert abs(original.width/original.height-16/9)<0.05
            original.verify()
        preview=BASE/'previews'/(id_+'.png')
        with Image.open(preview) as im:
            assert im.size==(384,216) and im.format=='PNG'
            im.verify()
    assert 'builtin_background_preview' in HEADER
    assert 'cp -a "$PROJECT/assets/themes/." "$KIT/assets/themes/"' in BUILD


def test_3_by_3_grid_and_explicit_confirmation():
    assert 'int gallery_focus_=0;' in UI
    assert 'const int row=i/3,col=i%3;' in UI
    assert 'if(input.nav==Direction::up && gallery_focus_>=3)delta=-3;' in UI
    assert 'input.nav==Direction::down && gallery_focus_+3' in UI
    assert 'if(delta){gallery_focus_+=delta;feedback.play(audio::Cue::focus);return;}' in UI
    assert 'if(input.is_pressed(Action::confirm) && gallery_focus_>=0' in UI
    assert 'save_gallery();feedback.play(audio::Cue::select);' in UI
    assert 'if(background){' in UI and 'return; // No triangle deletion or FTP import in backgrounds.' in UI
    assert 'gallery_choices_=ulg::bgh_gallery::builtins(ulg::bgh_gallery::Kind::background);' in UI
    assert 'Motiv mit Steuerkreuz waehlen' in UI
    assert 'Bilder: /data/homebrew/PPSA99079/user_themes/backgrounds' not in UI
    assert 'gallery_selection_loaded_after_boot_' in UI
    assert 'ulg::bgh_gallery::save_selection' in UI


def test_no_background_ftp_and_only_one_preview_per_frame():
    bg_open=UI.split('if(settings_subview_==6){',1)[1].split('}else if(settings_subview_==7){',1)[0]
    assert 'import_from_local_ftp' not in bg_open
    assert 'import_from_local_filesystem' not in bg_open
    loading=UI.split('if(settings_ && settings_subview_==6){',1)[1].split('}',1)[0]
    assert 'builtin_background_preview' in loading and 'covers_.load(preview);break;' in loading
    assert 'import_from_local_ftp' not in UI.split('void draw_settings_gallery_detail',1)[1]


def test_branch_is_consistent_with_build_and_installer():
    assert 'branches: [bgh-v1.1.25-background-gallery]' in WF
    assert "bgh-v1.1.25-background-gallery" in WF and "GITHUB_REF_NAME" in WF
    assert 'BGH V1.1.25' in UI
    assert 'V1_1_25_' in BUILD and 'V1_1_25_' in WF
    assert 'PPSA99079' in WF
    assert 'tests/test_v1125_background_grid.py' in WF
    assert 'BGH_V1.1.25/' in WF
