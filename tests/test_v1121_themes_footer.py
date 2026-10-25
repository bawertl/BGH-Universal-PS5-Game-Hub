"""V1.1.21 user-approved storage art, writable gallery folders, clean version display."""
from pathlib import Path
import importlib.util
import zipfile
import subprocess
import sys
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
GALLERY=(ROOT/'native_ps5/bgh_theme_gallery.hpp').read_text()
BUILD=(ROOT/'tooling/build_native_ps5.sh').read_text()
WORKFLOW=(ROOT/'.github/workflows/ps5-build.yml').read_text()

def test_short_version_and_crisp_native_font_rendering():
    assert 'constexpr const char *kBuildLabel = "BGH V1.1.26";' in UI
    assert 'const std::string release_label="BGH V1.1.26";' in UI
    assert '"Version: BGH V1.1.26"' in UI
    assert 'translated_text(list,fonts.semibold,kBuildLabel,' in UI
    assert 'translated_text(list,fonts.semibold,current_time_label()' in UI
    assert 'translated_text(list,fonts.regular,current_date_label()' in UI
    assert 'translated_text(list,fonts.semibold,music_settings_.enabled?' in UI
    assert 'translated_text(list,fonts.semibold,release_label,1720,1057,14' in UI
    for unsupported in ('UI-TEST','@BGH_GIT_COMMIT@','PS2-RPCS3-ICONS'):
        assert unsupported not in UI
    assert 'branches: [bgh-v1.1.26-fix1]' in WORKFLOW

def test_approved_storage_images_are_really_transparent():
    for filename in ('storage_internal.png','storage_usb.png'):
        with Image.open(ROOT/'assets/themes'/filename) as art:
            assert art.mode == 'RGBA' and art.size==(256,256)
            assert art.getchannel('A').getextrema()==(0,255)
        assert '/app0/assets/themes/'+filename in UI
    assert 'list.image(tex,' in UI

def test_install_gallery_folders_and_safe_legacy_compatibility():
    for name in ('backgrounds','logos'):
        assert (ROOT/'user_themes'/name/'.keep').is_file()
        assert 'BGH_GALLERY_UPLOAD_ROOT "/'+name+'"' in GALLERY
        assert '/app0/user_themes/'+name in GALLERY
        assert '/data/bgh/themes/'+name in GALLERY
    assert 'std::any_of(result.begin(),result.end()' in GALLERY
    assert 'const std::string id = "user:" + filename' in GALLERY
    assert 'approved = true' in GALLERY and 'image.path.c_str()' in GALLERY
    assert 'std::rename(temp.c_str(),path.c_str())' in GALLERY
    assert 'append_user_theme_folders.py' in BUILD
    assert 'user_themes/backgrounds/' in WORKFLOW and 'user_themes/logos/' in WORKFLOW

def test_append_folders_preserves_existing_native_zip_entries(tmp_path):
    module_path=ROOT/'tooling/append_user_theme_folders.py'
    spec=importlib.util.spec_from_file_location('themefolders',module_path)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    package=tmp_path/'native.zip'
    with zipfile.ZipFile(package,'w') as z:
        entry=zipfile.ZipInfo('PPSA99079/eboot.bin')
        entry.create_system=3
        entry.external_attr=(0o100777<<16)
        z.writestr(entry,b'\x7fSELF_TEST_SAMPLE')
    with zipfile.ZipFile(package) as z:
        before=z.read('PPSA99079/eboot.bin')
    module.append_folders(package,'PPSA99079',ROOT/'user_themes')
    module.append_folders(package,'PPSA99079',ROOT/'user_themes')
    with zipfile.ZipFile(package) as z:
        names=z.namelist()
        assert len(names)==len(set(names))
        assert z.read('PPSA99079/eboot.bin')==before
        for sub in ('backgrounds','logos'):
            for suffix in ('/', '/.keep'):
                name='PPSA99079/user_themes/'+sub+suffix
                assert name in names
                assert ((z.getinfo(name).external_attr>>16)&0o777)==0o777

def test_existing_backends_unmodified_contract():
    for needle in ('controllers_.observe_primary(input.connected);',
                   'controllers_.poll();', 'shadow_storage_.diagnostic',
                   'stage_ps5sx2_disc_launch(g.path)',
                   'make_registered_ps5_launch_plan(g.title_id)',
                   'make_registered_ps4_launch_plan(g.title_id)'):
        assert needle in UI
