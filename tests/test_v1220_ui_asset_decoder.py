"""V0.12.21: Guard the real native headers and the platform fan-cover header path.

This test intentionally does not use the host_shim headers: that mismatch caused
GitHub Actions to fail before the native PS5 build could begin.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def test_png_native_header_links_and_background_preserves_native_pixels(tmp_path):
    cpp = tmp_path / 'check_ui_png.cpp'
    cpp.write_text(r'''#include "cover_png.hpp"
#include <cassert>
int main(int argc,char **argv) {
    assert(argc==2);
    ulg::PngImage picture;
    assert(ulg::decode_png(argv[1],picture,0));
    assert(picture.width==1536 && picture.height==864);
    assert(picture.rgba.size()==1536u*864u*4u);
    assert(ulg::decode_png(argv[1],picture));
    assert(picture.width==512 && picture.height==512);
    return 0;
}
''')
    binary = tmp_path / 'check_ui_png'
    subprocess.run([
        'g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
        '-I', str(ROOT / 'native_ps5'), str(cpp),
        str(ROOT / 'native_ps5/cover_png.cpp'), '-lz', '-o', str(binary)
    ], check=True)
    subprocess.run([str(binary),str(ROOT/'assets/premium_lake_background.png')],check=True)



def test_native_cover_image_adapter_compiles_with_real_png_header(tmp_path):
    # GitHub downloads the pinned real stb_image.h for JPG/TGA. This local stub
    # supplies only those symbols, so we can validate the exact adapter/header
    # linkage without depending on an external network fetch.
    stb = tmp_path / 'stb_image.h'
    stb.write_text(r'''#pragma once
inline int stbi_info_from_memory(const unsigned char*, int, int*, int*, int*) { return 0; }
inline const char* stbi_failure_reason() { return "stub"; }
inline unsigned char* stbi_load_from_memory(const unsigned char*, int, int*, int*, int*, int) { return nullptr; }
inline void stbi_image_free(void*) {}
''')
    cpp = tmp_path / 'adapter.cpp'
    cpp.write_text(r'''#include "cover_image.hpp"
#include <cassert>
int main(int argc,char **argv) {
    assert(argc==2);
    ulg::PngImage picture;
    assert(ulg::decode_ui_image(argv[1],picture));
    assert(picture.width==1536 && picture.height==864);
    assert(ulg::decode_cover_image(argv[1],picture));
    assert(picture.width==512 && picture.height==512);
    return 0;
}
''')
    binary=tmp_path/'check_adapter'
    subprocess.run([
        'g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
        '-I',str(ROOT/'native_ps5'),'-I',str(tmp_path),str(cpp),
        str(ROOT/'native_ps5/cover_image.cpp'),
        str(ROOT/'native_ps5/cover_png.cpp'),'-lz','-o',str(binary)
    ],check=True)
    subprocess.run([str(binary),str(ROOT/'assets/premium_lake_background.png')],check=True)


def test_design_source_uses_real_lake_and_not_still_overlay():
    source=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
    assert 'covers_.get("/app0/assets/premium_lake_background.png")' in source
    assert 'covers_.load("/app0/assets/premium_lake_background.png")' in source
    assert 'covers_.get("/app0/assets/ui/dashboard_overlay.png")' not in source
    assert 'covers_.get("/app0/assets/lake_background.png")' not in source
    assert 'decode_ui_image(path,image,&decode_error)' in source
    assert 'fan_case_header_ps5.png' in source
    assert 'fan_case_header_ps4.png' in source
    assert 'fan_case_header_switch.png' in source
