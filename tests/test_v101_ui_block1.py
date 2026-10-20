"""V1.0.5: real pad-status fallback, recent art loading, and measured badge sizing."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
MONITOR=(ROOT/'native_ps5/controller_status.hpp').read_text()
WORKFLOW=(ROOT/'.github/workflows/ps5-build.yml').read_text()

def test_ui_input_is_grounded_primary_controller_status(tmp_path):
    assert 'controllers_.observe_primary(input.connected);' in SRC
    assert 'void observe_primary(bool connected){' in MONITOR
    assert 'state[0]=connected?1:0;' in MONITOR
    assert 'if(connected)' in SRC and 'list.glow({x-5,y-1,48,43}' in SRC
    assert 'state.fill(-1);' in MONITOR  # unseen secondary controllers stay unknown
    exe=tmp_path/'controller_monitor'
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror',
        '-I',str(ROOT/'native_ps5'), str(ROOT/'tests/controller_monitor_harness.cpp'),
        '-Wl,--export-dynamic','-ldl','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

def test_recent_cover_loading_precedes_regular_visible_list():
    fn=SRC.split('void load_next_visible_cover(){',1)[1].split('    void refresh()',1)[0]
    assert 'for(const int index:recent_indices())' in fn
    assert 'if(load_cover_for_game(catalogue_.games[static_cast<std::size_t>(index)]))return;' in fn
    assert fn.index('for(const int index:recent_indices())')<fn.index('if(indices_.empty())return;')
    assert fn.index('for(const int index:recent_indices())')<fn.index('enqueue(selected_);')
    assert 'load_cover_for_game(ulg::Game &g)' in SRC
    assert 'covers_.clear();' in SRC  # refreshing clears textures, then priority reloads them
    assert 'bad_cover_reimports_[g.id]++==0' in SRC

def test_platform_badge_uses_actual_serif_metrics():
    fn=SRC.split('void draw_game_info(gfx::DrawList &list) const {',1)[1].split('    std::array<int,3> recent_indices()',1)[0]
    # V1.0.8 intentionally removes the platform capsule; text remains uncut.
    assert 'serif_text(list,platform_text,755,698,20' in fn
    assert 'chip_padding' not in fn and 'chip_w' not in fn
    assert 'kSerifAtlasGlyphs' in SRC and 'glyph->advance*scale' in SRC

def test_branch_build_and_version_consistency():
    assert 'branches: [bgh-v1.1.5]' in WORKFLOW
    assert 'BGH V1.1.5-CONTAINER-COVER-FIX' in SRC
    assert 'BGH-V1-1-5-COVER-PPSA99079-TEST' in WORKFLOW
    assert 'V1_1_5_PPSA99079_PS5_TEST.zip' in WORKFLOW
    assert 'V1_1_5_${TITLE_ID}_PS5_TEST.zip' in (ROOT/'tooling/build_native_ps5.sh').read_text()
