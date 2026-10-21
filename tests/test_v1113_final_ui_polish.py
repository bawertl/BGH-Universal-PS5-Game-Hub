"""BGH 1.1.3 UI consolidation and safe folder editing."""
from pathlib import Path
from collections import deque

from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()

def _opaque_component_sizes(alpha_image, threshold=12):
    """4-connected alpha components without optional NumPy/SciPy dependencies."""
    width, height = alpha_image.size
    pixels = alpha_image.load()
    visited = bytearray(width * height)
    sizes = []
    for y in range(height):
        for x in range(width):
            start = y * width + x
            if visited[start] or pixels[x, y] <= threshold:
                continue
            queue = deque([(x, y)])
            visited[start] = 1
            size = 0
            while queue:
                px, py = queue.popleft()
                size += 1
                for nx, ny in ((px - 1, py), (px + 1, py), (px, py - 1), (px, py + 1)):
                    if nx < 0 or ny < 0 or nx >= width or ny >= height:
                        continue
                    index = ny * width + nx
                    if not visited[index] and pixels[nx, ny] > threshold:
                        visited[index] = 1
                        queue.append((nx, ny))
            sizes.append(size)
    return sorted(sizes, reverse=True)


def test_three_tabs_full_width_and_no_legacy_root_view():
    assert 'std::array<const char *,3> kSettingsTabs' in CPP
    assert '"Emulatoren & Spielordner", "Darstellung & Verhalten", "System & Hilfe"' in CPP
    draw=CPP.split('void draw_settings(app::Frame &frame) const {',1)[1].split('void draw_root_button_legend(',1)[0]
    assert 'if(settings_tab_==0)draw_settings_emulators(list);' in draw
    assert 'else if(settings_tab_==1)draw_settings_presentation(list);' in draw
    assert 'else draw_settings_system(list);' in draw
    assert 'draw_settings_roots(list)' not in draw

def test_folder_add_is_manual_and_scan_activation_preserved():
    update=CPP.split('void update_emulator_settings(',1)[1].split('void save_emulator_path(',1)[0]
    assert 'begin_emulator_path_editor(app,folder,true)' in update
    assert 'begin_emulator_path_editor(false,profile.rom_folders.size(),true)' in update
    assert 'Action::touch' in update
    assert 'Action::l3' in update
    assert 'toggle_emulator_folder_scan(profile,folder)' in update
    assert 'profile_folder_enabled(p,static_cast<std::size_t>(sub-1))' in CPP
    assert 'ulg::save_user_roots(next,kUserPathsFile)' in CPP
    assert 'previously_enabled' in CPP
    assert 'root.enabled=!root.enabled;' in CPP
    assert 'ui::Button::left_stick,hint("Scan AN/AUS")' in CPP

def test_dashboard_icon_fills_more_of_canvas_without_distortion():
    with Image.open(ROOT/'sce_sys/icon0.png') as im:
        assert im.size==(512,512)
        # At x=256, previously y=442 was dark background; new badge expands lower.
        assert any(channel > 35 for channel in im.convert('RGB').getpixel((256,442)))

def test_console_icons_have_no_distant_isolated_alpha_fragment():
    for name in ('ps4','xbox360'):
        with Image.open(ROOT/'assets/platforms'/f'{name}.png') as im:
            alpha=im.convert('RGBA').getchannel('A')
        sizes=_opaque_component_sizes(alpha)
        assert sizes and sizes[0]>1000
        assert all(v<=2 for v in sizes[1:])

def test_version_and_workflow():
    assert 'BGH V1.1.8-PLAYTIME-PER-GAME-SAFETY' in CPP
    assert 'release_label="BGH V1.1.8"' in CPP
    assert 'branches: [bgh-v1.1.8, main]' in WF
    assert 'BGH-V1-1-8-PLAYTIME-PPSA99079-TEST' in WF
