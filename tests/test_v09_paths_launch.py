"""Native PS5 registered-title launch validation and idempotent root migration."""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
CPP = ROOT / 'native_ps5'

def cxx_build(*sources):
    tmp = tempfile.TemporaryDirectory()
    executable = pathlib.Path(tmp.name) / 'test'
    cmd = ['c++', '-std=c++20', '-O0', '-Wall', '-Wextra', '-Werror', '-I', str(CPP),
           *[str(s) for s in sources], '-o', str(executable)]
    subprocess.run(cmd,check=True,capture_output=True,text=True)
    return tmp, executable

def test_register_ps5_ids_accept_only_native_ps5_titles():
    with tempfile.TemporaryDirectory() as t:
        cpp=pathlib.Path(t)/'check.cpp'
        cpp.write_text('''#include "ps5_launch.hpp"
#include <cassert>
int main(){
 using ulg::valid_native_ps5_title_id;
 assert(valid_native_ps5_title_id("PS5","PPSA01325"));
 assert(valid_native_ps5_title_id("PS5","PPSA28183"));
 assert(!valid_native_ps5_title_id("PS4","CUSA12345"));
 assert(!valid_native_ps5_title_id("PS5","PPSA99079"));
 assert(!valid_native_ps5_title_id("PS5","PPSA99008"));
 assert(!valid_native_ps5_title_id("PS5","PPSA12a45"));
 assert(!valid_native_ps5_title_id("PS5","PPSA1234"));
 assert(!ulg::launch_registered_ps5("CUSA11111").accepted);
}''')
        tmp,exe=cxx_build(cpp,CPP/'ps5_launch.cpp')
        try:subprocess.run([str(exe)],check=True,capture_output=True)
        finally:tmp.cleanup()

def test_root_migration_idempotent_and_preserves_old_entries():
    with tempfile.TemporaryDirectory() as t:
        cpp=pathlib.Path(t)/'check.cpp'
        cpp.write_text('''#include "root_manager.hpp"
#include <cassert>
#include <string>
#include <vector>
int main(){
 std::vector<ulg::Root> roots={{"PS5/PS4","/data/etaHEN/games"},{"PS2","/data/PCSX2/games"}};
 assert(ulg::ensure_native_roots(roots));
 assert(roots.size()==6);
 assert(roots[0].path=="/data/etaHEN/games");
 assert(roots[1].platform=="PS2");
 assert(!ulg::ensure_native_roots(roots));
 assert(roots.size()==6);
 assert(roots[2].platform=="PS5" && roots[2].path=="/data/homebrew");
 assert(roots[3].platform=="PS5" && roots[3].path=="/mnt/usb0");
 assert(roots[4].platform=="PS4" && roots[4].path=="/data/homebrew");
 assert(roots[5].platform=="PS4" && roots[5].path=="/mnt/usb0");
 roots[3].path="/mnt/usb1/homebrew";
 assert(!ulg::ensure_native_roots(roots));
 assert(roots[3].path=="/mnt/usb1/homebrew");
}''')
        tmp,exe=cxx_build(cpp,CPP/'root_manager.cpp',CPP/'library_core.cpp',CPP/'ftp_readonly.cpp')
        try:subprocess.run([str(exe)],check=True,capture_output=True)
        finally:tmp.cleanup()

def test_bundle_contains_new_native_launch_and_preserves_other_components():
    src = (CPP/'universal_launcher.cpp').read_text()
    assert 'launch_registered_ps5(g.title_id)' in src
    assert 'BGH V1.1.30' in src
    assert 'ulg_v08/covers' in src
    assert 'kTabs' in src
    assert 'save_user_roots(roots_,kUserPathsFile)' in src
    workflow=(ROOT/'.github/workflows/ps5-build.yml').read_text()
    assert 'bgh-v1.1.30-fix3' in workflow
    assert 'BGH_BawertLs_Game_Hub_V1_1_30_PPSA99079_PS5_TEST.zip' in workflow
    assert 'V1_1_30_PPSA99079_PS5_TEST.zip' in workflow
