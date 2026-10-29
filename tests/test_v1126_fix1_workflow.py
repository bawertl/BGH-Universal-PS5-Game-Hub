"""Regression guards for the V1.1.26 fix1 GitHub workflow.

These tests specifically prevent treating removed FTP-based theme imports as
required code and prevent version/branch/ZIP naming mismatches.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = (ROOT / '.github/workflows/ps5-build.yml').read_text()
UI = (ROOT / 'native_ps5/universal_launcher.cpp').read_text()
BUILD = (ROOT / 'tooling/build_native_ps5.sh').read_text()


def test_branch_name_and_visible_version_are_consistent():
    assert 'branches: [bgh-v1.1.32]' in WORKFLOW
    assert "test \"$GITHUB_REF_NAME\" = 'bgh-v1.1.32'" in WORKFLOW
    assert 'group: bgh-v1.1.32-${{ github.ref }}' in WORKFLOW
    assert 'BGH V1.1.32' in UI
    assert 'BGH V1.1.32-fix1' not in UI
    assert 'V1_1_32_${TITLE_ID}_PS5_TEST.zip' in BUILD


def test_removed_ftp_gallery_imports_are_never_required_to_exist():
    assert "if grep -Fq 'import_from_local_filesystem(kind)' native_ps5/universal_launcher.cpp ||" in WORKFLOW
    assert "grep -Fq 'import_from_local_ftp(kind)' native_ps5/universal_launcher.cpp; then" in WORKFLOW
    assert 'import_from_local_filesystem(kind)' not in UI
    assert 'import_from_local_ftp(kind)' not in UI
    assert 'gallery_choices_=ulg::bgh_gallery::builtins(kind);' in UI
    assert 'tests/test_v1126_logo_gallery.py' in WORKFLOW
    assert 'tests/test_v1126_fix1_workflow.py' in WORKFLOW


def test_backup_and_native_package_names_are_coherent():
    assert 'build/backup/BGH_V1.1.32/' in WORKFLOW
    assert 'BGH_V1.1.32_KOMPLETTPROJEKT.zip BGH_V1.1.32)' in WORKFLOW
    assert 'path: build/BGH_V1.1.32_KOMPLETTPROJEKT.zip' in WORKFLOW
    assert 'name: BGH-V1-1-32-PPSA99079' in WORKFLOW
    assert 'BGH_BawertLs_Game_Hub_V1_1_32_PPSA99079_PS5_TEST.zip' in WORKFLOW
    assert 'PPSA99079' in WORKFLOW
