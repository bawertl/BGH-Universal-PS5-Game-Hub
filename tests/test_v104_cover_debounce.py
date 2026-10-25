"""V1.0.5: avoid synchronous cover decoding during rapid list navigation."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / 'native_ps5/universal_launcher.cpp').read_text()
WORKFLOW = (ROOT / '.github/workflows/ps5-build.yml').read_text()


def test_real_time_policy_fast_navigation_waits_and_recovers(tmp_path):
    source = tmp_path / 'debounce.cpp'
    source.write_text(r'''
    #include "cover_load_debounce.hpp"
    #include <cassert>
    int main() {
        ulg::CoverLoadDebounce gate;
        assert(!gate.allow_regular_covers());
        // Regular 60Hz browsing never decodes while the user presses next.
        for (int i = 0; i < 100; ++i) {
            gate.advance(1.f / 60.f, true, i, 0);
            assert(!gate.allow_regular_covers());
        }
        // Two idle frames (33.3ms) are not enough; three (50ms) are.
        for (int i = 0; i < 2; ++i) {
            gate.advance(1.f / 60.f, false, 99, 0);
            assert(!gate.allow_regular_covers());
        }
        gate.advance(1.f / 60.f, false, 99, 0);
        assert(gate.allow_regular_covers());
        // Already-idle frames keep loading newly-needed covers one per frame.
        gate.advance(1.f / 60.f, false, 99, 0);
        assert(gate.allow_regular_covers());
        // Presses and platform changes always defer the next new decode.
        gate.advance(1.f / 60.f, true, 99, 0);
        assert(!gate.allow_regular_covers());
        gate.advance(0.10f, false, 99, 0);
        assert(gate.allow_regular_covers());
        gate.advance(0.1f, false, 99, 1);
        assert(!gate.allow_regular_covers());
        // Programmatic selection jumps and refresh resets are also guarded.
        gate.advance(0.10f, false, 99, 1);
        assert(gate.allow_regular_covers());
        gate.advance(0.1f, false, 17, 1);
        assert(!gate.allow_regular_covers());
        gate.reset();
        assert(!gate.allow_regular_covers());
        gate.advance(0.02f, false, 17, 1);
        assert(!gate.allow_regular_covers());
        gate.advance(0.10f, false, 17, 1);
        assert(gate.allow_regular_covers());
        // Invalid negative frame times cannot accidentally open the gate.
        gate.reset();
        gate.advance(-3.0f, false, 0, 0);
        gate.advance(-3.0f, false, 0, 0);
        assert(!gate.allow_regular_covers());
        return 0;
    }
    ''')
    exe = tmp_path / 'debounce'
    subprocess.run(['g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
        '-I', str(ROOT / 'native_ps5'), str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


def test_recent_cover_priority_and_refresh_unchanged():
    block = UI.split('void load_next_visible_cover(){', 1)[1].split('void refresh()', 1)[0]
    assert 'for(const int index:recent_indices())' in block
    assert block.index('for(const int index:recent_indices())') < block.index('if(!cover_load_debounce_.allow_regular_covers())return;')
    assert block.index('for(const int offset:{0,-1,1})') < block.index('if(!cover_load_debounce_.allow_regular_covers())return;')
    assert block.index('if(!cover_load_debounce_.allow_regular_covers())return;') < block.index('enqueue(selected_);')
    assert 'cover_load_debounce_.advance(dt,navigation_event,selected_,tab_);' in UI
    assert 'cover_load_debounce_.reset();' in UI
    assert 'if(!covers_.has("/app0/assets/ui/serif_atlas.png"))' in UI
    assert 'bool load_cover_for_game(ulg::Game &g)' in UI


def test_new_header_is_shipped_and_version_consistent():
    assert '"cover_load_debounce.hpp"' in UI
    assert "'cover_load_debounce.hpp'" in (ROOT / 'tooling/install_ui_kit_overlay.py').read_text()
    assert 'branches: [bgh-v1.1.29]' in WORKFLOW
    assert 'BGH V1.1.29' in UI
    assert 'release_label="BGH V1.1.9"' in UI
    assert 'BGH-V1-1-29-PPSA99079' in WORKFLOW
    assert 'V1_1_29_${TITLE_ID}_PS5_TEST.zip' in (ROOT / 'tooling/build_native_ps5.sh').read_text()
