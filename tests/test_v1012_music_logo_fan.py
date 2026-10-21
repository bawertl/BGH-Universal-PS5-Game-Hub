"""BGH V1.1.0: actual OGG music, saved toggle, logo and fan integration."""
from pathlib import Path
from PIL import Image
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
STATE=(ROOT/'native_ps5/state_store.cpp').read_text()
OVERLAY=(ROOT/'tooling/install_ui_kit_overlay.py').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()


def test_logo_real_files_and_gui_branding():
    with Image.open(ROOT/'sce_sys/icon0.png') as im:
        assert im.size == (512,512)
    with Image.open(ROOT/'assets/bgh_logo.png') as im:
        assert im.size == (512,512) and im.mode=='RGBA'
    with Image.open(ROOT/'assets/bgh_wordmark.png') as im:
        assert im.mode == 'RGBA' and im.size == (960,152)
    assert 'covers_.get("/app0/assets/bgh_wordmark.png")' in CPP
    assert "shutil.copy2(project/'assets'/'bgh_wordmark.png'" in OVERLAY


def test_center_and_release_updated_without_touching_scroll_debounce():
    assert 'kFanCentreX = (kGameListRight + (kScreenRight-kFanRightMargin))*0.5f + 42.0f' in CPP
    assert 'BGH V1.1.7-PLAYTIME-LOG-EXPERIMENT' in CPP
    assert 'release_label="BGH V1.1.7"' in CPP
    assert 'kQuietSeconds = 0.035f' in (ROOT/'native_ps5/cover_load_debounce.hpp').read_text()
    assert 'if(order_.size()>=256)' in CPP
    assert 'branches: [bgh-v1.1.7, main]' in WF
    assert 'BGH-V1-1-7-PLAYTIME-PPSA99079-TEST' in WF


def test_audio_ogg_is_decodable_48k_stereo():
    music=ROOT/'assets/audio/music/BGH_EccoTidesOfTime.ogg'
    assert music.read_bytes()[:4]==b'OggS'
    payload=music.read_bytes()
    ident=payload.find(b'\x01vorbis')
    assert ident>0, 'Vorbis identification header missing'
    assert payload[ident+11]==2, 'Expected stereo Vorbis'
    assert int.from_bytes(payload[ident+12:ident+16],'little')==48000
    assert "track->set_looping(playlist_.size()==1);" in OVERLAY
    assert 'bgm_enabled_from_storage' in OVERLAY
    assert 'if(bgm_enabled)music.pump(dt)' in OVERLAY


def test_music_settings_isolated_fourth_entry():
    assert 'struct MusicSettings' in (ROOT/'native_ps5/state_store.hpp').read_text()
    assert 'return save(path,"ULGM",w,status);' in STATE
    assert 'load(path,"ULGM",bytes,status)' in STATE
    assert 'save_music_setting(!music_settings_.enabled)' in CPP
    assert 'std::min(3,presentation_selected_+1)' in CPP
    assert 'music_settings_.enabled' in CPP
    assert 'kPrimaryMusicFile' in CPP and 'kFallbackMusicFile' in CPP
    for magic in ('ULGP','ULGE','ULGL','ULGT','ULGM'):
        assert f'"{magic}"' in STATE


def test_music_roundtrip_preserves_legacy_bytes_and_rejects_corruption(tmp_path):
    code=tmp_path/'music.cpp'
    code.write_text(r'''
#include "state_store.hpp"
#include <cassert>
#include <fstream>
#include <string>
std::string bytes(const std::string &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){
  assert(argc==2);
  const std::string base=argv[1];
  const auto prefs=base+"/preferences.bin", music=base+"/music.bin";
  ulg::Preferences legacy;legacy.favorites={"gameA"};legacy.recent={"gameB","gameC"};
  assert(ulg::write_preferences(legacy,prefs));
  const auto orig=bytes(prefs);
  ulg::MusicSettings default_val{};assert(default_val.enabled && default_val.revision==0);
  assert(ulg::write_music_settings({false,3},music));
  ulg::MusicSettings read{};assert(ulg::read_music_settings(music,read));
  assert(!read.enabled && read.revision==3);
  assert(bytes(prefs)==orig);
  assert(ulg::write_music_settings({true,4},music));
  assert(ulg::read_music_settings(music,read));
  assert(read.enabled && read.revision==4);
  {std::ofstream file(music,std::ios::binary|std::ios::app);file.put('!');}
  ulg::MusicSettings sentinel{false,37};
  assert(!ulg::read_music_settings(music,sentinel));
  assert(!sentinel.enabled && sentinel.revision==37);
  assert(bytes(prefs)==orig);
}
''')
    exe=tmp_path/'test_music'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
                    '-I'+str(ROOT/'native_ps5'),str(code),str(ROOT/'native_ps5/state_store.cpp'),
                    '-o',str(exe)],check=True)
    subprocess.run([str(exe),str(tmp_path)],check=True)
