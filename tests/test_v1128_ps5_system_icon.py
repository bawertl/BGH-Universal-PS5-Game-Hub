"""BGH V1.1.31: full-bleed PS5 system icon and package copy verification."""
from io import BytesIO
from pathlib import Path
import hashlib
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
ICON=ROOT/'sce_sys/icon0.png'
EXPECTED="17b7104e4681ad78d86504259dd25a6e4ba70e0a59d1bdc46cff8d91e966ae7d"


def test_ps5_square_art_is_current_and_fills_all_512_pixels():
    binary=ICON.read_bytes()
    assert hashlib.sha256(binary).hexdigest()==EXPECTED
    with Image.open(BytesIO(binary)) as im:
        assert im.format=='PNG' and im.size==(512,512) and im.mode=='RGB'
        assert len(im.getcolors(512*512) or [])>256
        # No black padding bars inserted. The actual generated artwork
        # reaches all four edges (a full-bleed, not letterboxed tile).
        for edge in (im.crop((0,0,512,1)),im.crop((0,511,512,512)),
                     im.crop((0,0,1,512)),im.crop((511,0,512,512))):
            assert len(edge.getcolors(512) or [])>10


def test_system_icon_installed_with_title_id_and_matched_by_workflow():
    shell=(ROOT/'tooling/build_native_ps5.sh').read_text()
    wf=(ROOT/'.github/workflows/ps5-build.yml').read_text()
    ui=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
    assert 'cp "$PROJECT/sce_sys/icon0.png" "$KIT/sce_sys/icon0.png"' in shell
    assert "z.read(prefix + 'sce_sys/icon0.png') != source" in wf
    assert 'branches: [bgh-v1.1.31-fix1]' in wf
    assert 'BGH V1.1.31' in ui
    assert 'PPSA99079' in wf and 'PPSA99079' in shell
    assert 'tests/test_v1128_ps5_system_icon.py' in wf
