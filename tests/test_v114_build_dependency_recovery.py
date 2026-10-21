"""Regression guards for formerly unavailable pinned UI-kit/OpenGL dependencies."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]


def test_ui_kit_source_is_pinned_and_has_mirror():
    source=(ROOT/'tooling/fetch_pinned_ui_kit.sh').read_text()
    build=(ROOT/'tooling/build_native_ps5.sh').read_text()
    assert 'mihawk-99/PS5_VKHomebrewUI.git' in source
    assert 'bbe19054ee2e7ec069254775273aeb8846c4bd3e' in build
    assert 'bash "$PROJECT/tooling/fetch_pinned_ui_kit.sh"' in build
    assert 'git clone --filter=blob:none https://github.com/blackbearreloaded/' not in build


def test_opengl_sdk_is_actually_built_not_copied():
    source=(ROOT/'tooling/build_pinned_opengl_sdk.sh').read_text()
    workflow=(ROOT/'.github/workflows/ps5-build.yml').read_text()
    assert 'a519561626933257213d64a4a8bd5e02b55dceb2' in source
    assert 'make -C "$SOURCE" sdk-gl46' in source
    assert 'manifest.sha256' in source
    assert 'PS5_OPENGL_PREFIX=$prefix' in workflow
    assert 'actions/cache/restore@v4' in workflow
    assert 'actions/cache/save@v4' in workflow
    assert '/usr/bin/python3 -m pytest -q tests' in workflow
    assert 'Sicherstellen, dass wirklich V1.1.8 gebaut wird' in workflow


def test_shell_syntax():
    for name in ('tooling/fetch_pinned_ui_kit.sh','tooling/build_pinned_opengl_sdk.sh','tooling/build_native_ps5.sh'):
        subprocess.run(['bash','-n',str(ROOT/name)],check=True)


def test_invalid_sdk_manifest_is_rejected_before_build(tmp_path):
    sdk=tmp_path/'sdk'
    sdk.mkdir()
    (sdk/'manifest.sha256').write_text('a'*64+'  lib/libPS5OpenGL.a\n')
    kit=tmp_path/'kit'
    (kit/'tools').mkdir(parents=True)
    (kit/'tools/setup-native-dependencies.sh').write_text('#!/bin/bash\n')
    result=subprocess.run(['bash',str(ROOT/'tooling/build_pinned_opengl_sdk.sh'),str(kit),str(sdk)],text=True,capture_output=True,timeout=10)
    assert result.returncode==22
    assert 'failed validation' in result.stderr
