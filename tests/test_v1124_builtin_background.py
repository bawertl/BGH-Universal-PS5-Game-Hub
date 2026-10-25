"""V1.1.24 built-in gaming hall must be selectable without FTP or file access."""
from pathlib import Path
import re
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
ASSET=ROOT/'assets/themes/backgrounds/gaming_hall_2k.png'
HEADER=(ROOT/'native_ps5/bgh_theme_gallery.hpp').read_text()
BUILD=(ROOT/'tooling/build_native_ps5.sh').read_text()
WORKFLOW=(ROOT/'.github/workflows/ps5-build.yml').read_text()
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()

def test_2k_art_is_bundled_and_in_valid_png_format():
    with Image.open(ASSET) as im:
        assert im.format=='PNG'
        assert im.size==(2560,1440)
        im.verify()
    assert 'cp -a "$PROJECT/assets/themes/." "$KIT/assets/themes/"' in BUILD
    assert "'assets/themes/backgrounds/gaming_hall_2k.png'" in WORKFLOW

def test_gallery_contains_nine_stable_builtin_backgrounds():
    bg=HEADER.split('if (kind == Kind::background) return {',1)[1].split('};',1)[0]
    entries=re.findall(r'\{\"([^\"]+)\", \"([^\"]+)\", false, (?:true|false)\}',bg)
    assert [name for name,_ in entries]==['lake','showroom','mountain_crown','gaming_hall','neon_corridor','metal_blue','neon_wall','orbital_horizon','blue_hall']
    assert entries[3][1]=='/app0/assets/themes/backgrounds/gaming_hall_2k.png'
    assert 'std::vector<ThemeItem> result = builtins(kind);' in HEADER
    assert 'gallery_choices_=ulg::bgh_gallery::builtins(ulg::bgh_gallery::Kind::background);' in UI
    assert 'save_gallery();feedback.play(audio::Cue::select);' in UI
    assert 'const auto background=gallery_selection_loaded_after_boot_?' in UI
    assert 'BGH V1.1.27' in UI
