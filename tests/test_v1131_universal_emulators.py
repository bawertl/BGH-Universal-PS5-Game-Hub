"""BGH V1.1.32: migration + emulator identity + carousel structural guards."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
REG=(ROOT/'native_ps5/platform_registry.hpp').read_text()
LC=(ROOT/'native_ps5/launcher_client.cpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()

def test_navigation_wraps_and_does_not_modify_design():
    assert '(selected_+delta+static_cast<int>(indices_.size()))%static_cast<int>(indices_.size())' in CPP
    assert '(recent_selected_+2)%3' in CPP
    assert '(recent_selected_+1)%3' in CPP

def test_emulator_id_is_not_discarded_and_no_old_id_fallback():
    assert 'active_emulator_title_id(p)' in CPP
    assert 'if(!p.app_path.empty())return {};' in CPP
    assert 'Konfigurierter Emulator nicht erkannt' in CPP
    assert 'make_launch_plan("Switch",game.path,id)' in CPP
    assert 'PPSA99186' in (ROOT/'assets/emulator_profiles.json').read_text()
    assert 'PPSA99186' in (ROOT/'native_ps5/emulator_detection.hpp').read_text()
    assert 'PPSA99186' in (ROOT/'native_ps5/library_core.cpp').read_text()

def test_registry_3_roundtrip_and_legacy_2(tmp_path):
    source=tmp_path/'registry.cpp'
    source.write_text(r'''
#include "platform_registry.hpp"
#include <cassert>
#include <cstdio>
#include <string>
int main(int,char**argv){
  auto p=ulg::default_platform_registry();
  for(auto &e:p.entries)if(e.id=="Switch"){
    e.app_path="/data/homebrew/PPSA99186";
    e.detected_title_id="PPSA99186";
    e.detected_name="ForgeNX";
    e.launch_protocol="eden_rom_argv";
    assert(ulg::active_emulator_title_id(e)=="PPSA99186");
  }
  assert(ulg::write_platform_registry(p,argv[1]));
  ulg::PlatformRegistry out;
  assert(ulg::read_platform_registry(argv[1],out));
  for(const auto &e:out.entries)if(e.id=="Switch"){
    assert(e.detected_title_id=="PPSA99186");
    assert(e.detected_name=="ForgeNX");
    assert(e.launch_protocol=="eden_rom_argv");
  }
  std::string legacy="ULGR|2|1\n";
  for(const auto &e:p.entries)legacy+=e.id+"|1|0||"+e.app_path+"|\n";
  char checksum[16];std::snprintf(checksum,sizeof checksum,"%08X",ulg::platform_checksum(legacy));
  auto f=std::fopen(argv[2],"wb");assert(f);
  legacy+="CRC|"+std::string(checksum)+"\n";
  assert(std::fwrite(legacy.data(),1,legacy.size(),f)==legacy.size());assert(!std::fclose(f));
  ulg::PlatformRegistry old;assert(ulg::read_platform_registry(argv[2],old));
  for(const auto &e:old.entries)if(e.id=="Switch"){
    assert(e.app_path=="/data/homebrew/PPSA99186");
    assert(e.detected_title_id.empty());
    assert(e.launch_protocol=="auto");
  }
}
''')
    exe=tmp_path/'regtest'
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-I',str(ROOT/'native_ps5'),str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(tmp_path/'third.bin'),str(tmp_path/'second.bin')],check=True)

def test_workflow_identity():
    assert 'branches: [bgh-v1.1.32]' in WF
    assert 'BGH V1.1.32' in CPP
