"""V1.0.12 regression: non-cropping full UV art and responsive bounded loading."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
DEBOUNCE=(ROOT/'native_ps5/cover_load_debounce.hpp').read_text()
WORKFLOW=(ROOT/'.github/workflows/ps5-build.yml').read_text()

def test_cover_display_preserves_printed_edges_without_distortion():
    fan=CPP.split('void draw_fan_cover(',1)[1].split('void draw_cover_fan(',1)[0]
    assert 'const gfx::Rect image_rect=with_case_header?art_rect:' in fan
    assert 'cover_contain_rect(size.first,size.second,art_rect)' in fan
    assert 'uv_cover_fit(size.first,size.second,art_rect):gfx::kFullUv' in fan
    assert 'list.image(image,image_rect,image_uv,' in fan
    assert 'const bool with_case_header=fan_cover_uses_case_header(g.platform);' in fan
    assert 'platform=="PS5" || platform=="PS4" || platform=="Switch"' in CPP

def test_contain_rect_preserves_dimensions_and_centers_poster():
    fn=CPP.split('gfx::Rect cover_contain_rect(',1)[1].split('class CoverTextures',1)[0]
    assert 'card.w/source_aspect' in fn
    assert 'card.h*source_aspect' in fn
    assert 'card.y+(card.h-height)*0.5f' in fn
    assert 'card.x+(card.w-width)*0.5f' in fn
    # Numerical examples verify geometry behind the implementation.
    for iw,ih,w,h in [(600,750,308,421),(750,1000,308,421),(1920,1080,308,421),(400,800,308,421)]:
        source=iw/ih
        if source>w/h:
            rw,rh=w,w/source
        else:
            rw,rh=h*source,h
        assert abs(rw/rh-source)<1e-6
        assert 0<rw<=w+1e-6 and 0<rh<=h+1e-6

def test_one_heavy_cover_operation_per_update_and_priority():
    fn=CPP.split('void load_next_visible_cover(){',1)[1].split('void refresh()',1)[0]
    assert fn.index('for(const int index:recent_indices())')<fn.index('for(const int offset:{0,-1,1})')
    assert fn.index('for(const int offset:{0,-1,1})')<fn.index('if(!cover_load_debounce_.allow_regular_covers())return;')
    assert 'if(load_cover_for_game(g))return;' in fn
    assert 'if(order_.size()>=256)' in CPP
    assert 'kQuietSeconds = 0.035f' in DEBOUNCE

def test_release_and_previous_startpaths_unchanged():
    assert 'branches: [bgh-v1.1.9, main]' in WORKFLOW
    assert 'BGH-V1-1-9-EMULATORS-PPSA99079-TEST' in WORKFLOW
    assert 'BGH V1.1.9-EMULATOR-AUTODETECTION' in CPP
    assert 'release_label="BGH V1.1.9"' in CPP
    for key in ('stage_ps5sx2_disc_launch(g.path)', 'make_registered_ps4_launch_plan(g.title_id)',
                'make_registered_ps5_launch_plan(g.title_id)', 'make_prosperoeden_menu_launch_plan()',
                'make_ps5x360_menu_launch_plan()'):
        assert key in CPP

def test_compiled_contain_geometry_uses_original_source(tmp_path):
    body=CPP.split('gfx::Rect cover_contain_rect(',1)[1].split('class CoverTextures',1)[0]
    source=tmp_path/'fit.cpp'
    source.write_text('''
    #include <cassert>
    #include <cmath>
    #include <algorithm>
    namespace gfx { struct Rect { float x,y,w,h; }; }
    gfx::Rect cover_contain_rect('''+body+'''
    int main(){
        const gfx::Rect card{0,0,308,421};
        const auto w=cover_contain_rect(600,750,card);
        assert(w.w>0 && w.h>0 && w.w<=308 && w.h<=421);
        assert(std::abs(w.w/w.h-0.8f)<0.0001f);
        assert(std::abs(w.x-(308-w.w)*0.5f)<0.0001f);
        assert(std::abs(w.y-(421-w.h)*0.5f)<0.0001f);
        const auto t=cover_contain_rect(400,800,card);
        assert(std::abs(t.w/t.h-0.5f)<0.0001f);
        assert(t.w<308 && t.h==421);
        const auto invalid=cover_contain_rect(0,0,card);
        assert(invalid.w==308 && invalid.h==421);
    }
    ''')
    exe=tmp_path/'fit'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
