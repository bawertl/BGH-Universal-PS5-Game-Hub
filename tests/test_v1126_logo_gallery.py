"""V1.1.26: bundled BGH logo gallery with integrated wordmark handling."""
from pathlib import Path
import re
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
HEADER=(ROOT/'native_ps5/bgh_theme_gallery.hpp').read_text()
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
BASE=ROOT/'assets/themes/logos'
IDS=(
    'legacy','crown','a01_chrom_orbit','a02_brush_neon','a03_tech_krone',
    'a04_gold_edel','a05_controller','b01_chrom_wappen','b02_brush_blue',
    'b03_multikonsole','b04_gold_krone','b05_neon_rahmen','c01_chrom_krone',
    'c02_brush_krone','c03_klassisch','c04_planet','c05_edge_tech','none'
)


def test_every_logo_is_embedded_and_previewed():
    block=HEADER.split('return {',2)[2].split('};',1)[0]
    items=re.findall(r'\{"([^"]+)", "([^"]*)", false, false\}',block)
    assert tuple(i for i,_ in items)==IDS
    for id_,path in items:
        if id_=='none':
            assert path==''
            continue
        source=ROOT/path.removeprefix('/app0/')
        assert source.is_file() and source.stat().st_size>5000
        with Image.open(source) as im:
            assert im.format=='PNG'
            assert im.width >= 512
            assert 'A' in im.getbands()
            im.verify()
        if id_ not in {'legacy','crown'}:
            preview=BASE/'previews'/(id_+'.png')
            with Image.open(preview) as im:
                assert im.size==(420,180)
                assert im.format=='PNG'
                im.verify()
    assert 'builtin_logo_preview' in HEADER
    assert 'logo_has_embedded_wordmark' in HEADER


def test_logo_gallery_uses_five_columns_and_no_separate_wordmark_for_new_logos():
    assert 'const int columns=5;' in UI
    assert 'gallery_choices_=ulg::bgh_gallery::builtins(kind);' in UI
    assert 'Logo mit Steuerkreuz waehlen' in UI
    assert 'ulg::bgh_gallery::logo_has_embedded_wordmark(gallery_selection_.logo)' in UI
    assert '{30,175,74,74}' in UI and '{109,190,323,52}' in UI
