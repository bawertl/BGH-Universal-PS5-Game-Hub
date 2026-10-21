"""Static release guard: version identity and separate USB repair branch."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_release_identity_and_caches():
    wf = (ROOT / '.github/workflows/ps5-build.yml').read_text()
    script = (ROOT / 'tooling/build_native_ps5.sh').read_text()
    ui = (ROOT / 'native_ps5/universal_launcher.cpp').read_text()
    assert 'branches: [bgh-v1.1.7, main]' in wf
    assert '"$GITHUB_REF_NAME" != "bgh-v1.1.7" && "$GITHUB_REF_NAME" != "main"' in wf
    assert 'BGH-V1-1-7-PLAYTIME-PPSA99079-TEST' in wf
    assert 'BGH_BawertLs_Game_Hub_V1_1_7_PPSA99079_PS5_TEST.zip' in wf
    assert 'BGH_BawertLs_Game_Hub_V1_1_7_${TITLE_ID}_PS5_TEST.zip' in script
    assert 'BGH V1.1.7-PLAYTIME-LOG-EXPERIMENT' in ui
    assert 'release_label="BGH V1.1.7"' in ui
    assert 'Version: BGH V1.1.7 Playtime-Log-Experimental' in ui
    assert 'library_v116_container_start_title.bin' in ui
    assert 'library_v113_ps5_cover.bin' in ui
    assert 'PPSA99079' in script


def test_external_build_dependencies_are_pinned_and_separate_from_bgh_tests():
    wf = (ROOT / '.github/workflows/ps5-build.yml').read_text()
    build = (ROOT / 'tooling/build_native_ps5.sh').read_text()
    assert '/usr/bin/python3 -m pytest -q tests' in wf
    assert 'bgh-opengl-build-venv/bin/python' in wf
    assert 'bash tooling/build_pinned_opengl_sdk.sh' in wf
    assert 'PS5_OPENGL_PREFIX=$prefix' in wf
    assert 'bash "$PROJECT/tooling/fetch_pinned_ui_kit.sh"' in build
