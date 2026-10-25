"""BGH V1.1.30 PS5 home-screen hero background and build package guards."""
import hashlib
import subprocess
import sys
from io import BytesIO
from pathlib import Path
from PIL import Image, ImageChops, ImageStat

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tooling'))
from verify_ps5_home_background import verify_dds_bytes, verify_paths, DDS_LENGTH


def test_ps5_home_background_both_valid_bc7_dds():
    a=ROOT/'sce_sys/pic0.dds'
    b=ROOT/'sce_sys/pic1.dds'
    verify_paths(a,b)
    assert a.stat().st_size==DDS_LENGTH
    assert a.read_bytes()==b.read_bytes()
    # PIL has an independent BC7 reader, so this also exercises block decoding.
    with Image.open(a) as decoded:
        assert decoded.format=='DDS'
        assert decoded.size==(3840,2160)
        assert decoded.getpixel((1350,900))[3]==255


def test_bc7_hero_matches_approved_source_artwork():
    source=ROOT/'design/ps5_home_hero_source.png'
    assert source.stat().st_size > 100000
    with Image.open(source) as original, Image.open(ROOT/'sce_sys/pic0.dds') as dds:
        approved=original.convert('RGB').resize((3840,2160),Image.Resampling.LANCZOS)
        recon=dds.convert('RGB')
        delta=ImageStat.Stat(ImageChops.difference(approved,recon))
        assert max(delta.mean)<3.0, 'The selection hero is not the approved artwork'


def test_ps5_home_hero_integrated_into_real_ps5_package_build():
    shell=(ROOT/'tooling/build_native_ps5.sh').read_text()
    workflow=(ROOT/'.github/workflows/ps5-build.yml').read_text()
    assert 'make -C "$KIT" init TITLE_ID="$TITLE_ID"' in shell
    assert 'for background in pic0.dds pic1.dds; do' in shell
    assert 'cp "$PROJECT/sce_sys/$background" "$KIT/sce_sys/$background"' in shell
    assert 'make -C "$KIT" BUILD_JOBS=' in shell
    assert "for art in ('pic0.dds','pic1.dds')" in workflow
    assert "z.read(target) != source_bg" in workflow
    assert 'python3 tooling/verify_ps5_home_background.py' in workflow
    assert 'tests/test_v1130_ps5_home_background.py' in workflow
    assert 'bgh-v1.1.30-fix1' in workflow
    assert 'cp -a native_ps5 tooling assets design' in workflow
