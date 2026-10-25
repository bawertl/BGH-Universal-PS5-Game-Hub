"""V1.0.12: dynamic original-aspect cover card and independent frosted theme state."""
from pathlib import Path
from PIL import Image
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
STATE=(ROOT/'native_ps5/state_store.cpp').read_text()
SHIM=(ROOT/'tooling/host_shim/concepts/state_store.hpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()


def test_native_ratio_with_case_headers_preserved():
    fan=CPP.split('void draw_fan_cover(',1)[1].split('void draw_cover_fan(',1)[0]
    assert 'fan_native_card_height(cover_dimensions.first,cover_dimensions.second,w,h)' in fan
    assert 'if(!with_case_header && covers_.has(g.cover_path))' in fan
    assert 'const gfx::Rect image_rect=with_case_header?art_rect:' in fan
    assert 'cover_contain_rect(size.first,size.second,art_rect)' in fan
    assert 'uv_cover_fit(size.first,size.second,art_rect):gfx::kFullUv' in fan
    assert 'const float near_distance=' in fan
    assert 'const float far_distance=' in fan
    assert 'if(order_.size()>=256)' in CPP


def test_theme_ui_and_storage_isolation():
    assert 'std::min(3,presentation_selected_+1)' in CPP
    assert 'presentation_selected_==2' in CPP
    assert 'save_theme_setting(!theme_settings_.frosted_glass)' in CPP
    assert 'kPrimaryThemeFile="/download0/hui/ulg_v04/theme.bin"' in CPP
    assert 'kFallbackThemeFile="/data/bgh/theme.bin"' in CPP
    assert 'if(primary_theme_ok)theme_settings_=primary_theme;' in CPP
    assert 'fallback_theme.revision>primary_theme.revision' in CPP
    assert 'write_theme_settings' in STATE and 'read_theme_settings' in STATE
    assert 'save(path,"ULGT",w,status)' in STATE
    assert 'theme_settings_.frosted_glass' in CPP
    assert 'struct ThemeSettings' in SHIM
    for x in ('ULGP','ULGE','ULGL','ULGT'):
        assert x in STATE


def test_preblurred_artifact_and_build_branch():
    with Image.open(ROOT/'assets/ui/frosted_lake_backdrop.png') as im:
        assert im.size==(1536,864)
        assert im.mode=='RGBA'
    glass=CPP.split('void glass_card(',1)[1].split('void draw_recent_cards(',1)[0]
    assert 'frosted_lake_backdrop.png' in glass
    assert 'lake_uv{r.x/1920.0f,r.y/1080.0f,r.w/1920.0f,r.h/1080.0f}' in glass
    assert 'list.image(blurred,r,lake_uv' in glass
    assert 'branches: [bgh-v1.1.29]' in WF
    assert 'BGH-V1-1-29-PPSA99079' in WF
    assert 'BGH V1.1.29' in CPP


def test_compiled_ratio_geometry_handles_normal_extreme_and_invalid(tmp_path):
    snippet=CPP.split('float fan_native_card_height(',1)[1].split('class CoverTextures',1)[0]
    src=tmp_path/'test.cpp'
    src.write_text('''
#include <cmath>
#include <algorithm>
#include <cassert>
float fan_native_card_height('''+snippet+'''
int main(){
  // 0.80 source -> 308/0.8=385: no black vertical bars.
  auto h=fan_native_card_height(600,750,308,421);
  assert(std::abs(h-385)<0.001f);
  // Narrower DVD art -> taller card.
  assert(std::abs(fan_native_card_height(708,1000,308,421)-308/0.708f)<0.01f);
  // Extreme aspect is bounded, with contain fallback for the artwork.
  assert(fan_native_card_height(1600,400,308,421)>=421*0.72f);
  assert(fan_native_card_height(200,1000,308,421)<=421*1.24f);
  assert(fan_native_card_height(0,750,308,421)==421);
}
''')
    exe=tmp_path/'geometry'
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_compiled_theme_persistence_independent_and_checksum(tmp_path):
    src=tmp_path/'storage.cpp'
    src.write_text('''
#include "state_store.hpp"
#include <cassert>
#include <fstream>
#include <string>
int main(int argc,char**argv){
    assert(argc==2);
    const std::string p=argv[1];
    ulg::ThemeSettings original{};
    assert(original.frosted_glass && original.revision==0);
    original.frosted_glass=false;original.revision=11;
    assert(ulg::write_theme_settings(original,p));
    ulg::ThemeSettings restored{};
    assert(ulg::read_theme_settings(p,restored));
    assert(!restored.frosted_glass && restored.revision==11);
    // Invalid checksum must not overwrite the caller's existing state.
    { std::ofstream out(p,std::ios::binary|std::ios::app);out.put('x'); }
    ulg::ThemeSettings sentinel{true,99};
    assert(!ulg::read_theme_settings(p,sentinel));
    assert(sentinel.frosted_glass && sentinel.revision==99);
}
''')
    exe=tmp_path/'storage'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
                    '-I'+str(ROOT/'native_ps5'),str(src),str(ROOT/'native_ps5/state_store.cpp'),
                    '-o',str(exe)],check=True)
    subprocess.run([str(exe),str(tmp_path/'theme.bin')],check=True)
