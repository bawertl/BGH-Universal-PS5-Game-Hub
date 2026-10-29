"""Regression: PS5 sce_sys icon format and package icon consistency."""
from io import BytesIO
from pathlib import Path
import hashlib
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
ICON=ROOT/'sce_sys/icon0.png'
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
BUILD=(ROOT/'tooling/build_native_ps5.sh').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()
EXPECTED_SHA256="17b7104e4681ad78d86504259dd25a6e4ba70e0a59d1bdc46cff8d91e966ae7d"


def test_new_home_icon_exact_file_is_present_and_square():
    data=ICON.read_bytes()
    assert hashlib.sha256(data).hexdigest()==EXPECTED_SHA256
    with Image.open(BytesIO(data)) as icon:
        assert icon.format=='PNG'
        assert icon.size==(512,512)
        assert icon.mode in ('RGB','RGBA')
        # Artwork is real content, not a solid-color placeholder.
        assert len(icon.getcolors(512*512) or [])>256
    with Image.open(BytesIO(data)) as fresh:
        fresh.verify()


def test_launcher_and_galleries_are_unchanged_in_function():
    assert 'constexpr const char *kBuildLabel = "BGH V1.1.32";' in UI
    assert 'const std::string release_label="BGH V1.1.32";' in UI
    assert 'gallery_choices_=ulg::bgh_gallery::builtins(kind);' in UI
    assert 'gallery_choices_=ulg::bgh_gallery::builtins(ulg::bgh_gallery::Kind::background);' in UI
    assert 'stage_ps5sx2_disc_launch(g.path)' in UI
    assert 'controllers_.poll();' in UI


def test_git_workflow_packaging_checks_new_app_icon():
    assert 'branches: [bgh-v1.1.32]' in WF
    assert 'test "$GITHUB_REF_NAME" = \'bgh-v1.1.32\'' in WF
    assert 'BGH_BawertLs_Game_Hub_V1_1_32_' in BUILD
    assert 'cp "$PROJECT/sce_sys/icon0.png" "$KIT/sce_sys/icon0.png"' in BUILD
    assert "z.read(prefix + 'sce_sys/icon0.png') != source" in WF
    assert 'tests/test_v1127_ps5_home_icon.py' in WF
