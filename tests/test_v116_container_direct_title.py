"""V1.1.8 container launch and appmeta title regression checks (host only)."""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / 'native_ps5'


def test_container_display_metadata_and_fallback(tmp_path):
    probe = tmp_path / 'title_probe.cpp'
    probe.write_text(r'''
#include "library_core.hpp"
#include <iostream>
int main(){
  std::cout << ulg::ps5_container_fallback_title("PPSA06084 Superliminal","PPSA06084") << '\n';
  std::cout << ulg::ps5_container_fallback_title("PPSA08804 - Sonic Racing","PPSA08804") << '\n';
  std::cout << ulg::ps5_container_fallback_title("REANIMAL-PPSA23372","PPSA23372") << '\n';
  std::cout << ulg::ps5_container_fallback_title("PPSA17221","PPSA17221") << '\n';
  std::cout << ulg::ps5_container_fallback_title("HelloPPSA06084Test","PPSA06084") << '\n';
  std::cout << ulg::ps5_appmeta_title_from_json(R"({"localizedParameters":{"en-US":{"titleName":"The Midnight Walk"},"de-DE":{"titleName":"Der Mitternachtsspaziergang"}}})") << '\n';
  std::cout << ulg::ps5_appmeta_title_from_json("{\"titleName\":\"Superliminal\"}") << '\n';
  std::cout << ulg::ps5_appmeta_title_from_json("{\"titleName\":42}") << '\n';
}
''')
    binary = tmp_path / 'title_probe'
    subprocess.run(['g++','-std=c++20','-O2','-I',str(CPP),str(probe),str(CPP/'library_core.cpp'),'-o',str(binary)],check=True)
    result = subprocess.run([str(binary)],capture_output=True,text=True,check=True)
    assert result.stdout.splitlines() == [
        'Superliminal', 'Sonic Racing', 'REANIMAL', 'PPSA17221',
        'HelloPPSA06084Test', 'Der Mitternachtsspaziergang', 'Superliminal', ''
    ]


def test_forwarder_only_for_container_not_native_fallback():
    cpp = (CPP/'universal_launcher.cpp').read_text()
    start = cpp.index('else if(g.platform=="PS5" && (g.format=="ffpfsc" || g.format=="exfat"))')
    end = cpp.index('else if(g.platform=="PS5")',start)
    branch = cpp[start:end]
    assert 'make_registered_ps5_launch_plan(g.title_id)' in branch
    assert 'ulg::send_launch(ps5_plan)' in branch
    assert 'rollback_recent(previous_recent)' in branch
    assert 'LaunchStatus::no_launcher' in branch
    assert 'launch_registered_ps5(' not in branch
    assert 'sys::quit()' in branch
    assert 'const bool ps5_direct=(g.platform=="PS5"' in cpp
    assert 'g.available && (plan.supported || ps5_direct' in cpp
    assert 'stage_ps5sx2_disc_launch(g.path)' in cpp


def test_both_scanners_read_appmeta_titles_and_preserve_covers():
    local = (CPP/'library_core.cpp').read_text()
    ftp = (CPP/'ftp_scan.cpp').read_text()
    assert 'ps5_appmeta_title_local(id)' in local
    assert 'ps5_container_fallback_title(title_from(c.name),id)' in local
    assert 'ps5_container_fallback_title(title_from_name(entry.name),id)' in ftp
    assert 'ftp.retrieve("/user/appmeta/"+game.title_id+"/param.json"' in ftp
    assert 'container_cover_budget=128' in ftp
    assert 'candidates.push_back("/user/appmeta/"+game.title_id+"/icon0.png")' in ftp


def test_release_branch_and_cache():
    ui = (CPP/'universal_launcher.cpp').read_text()
    workflow = (ROOT/'.github/workflows/ps5-build.yml').read_text()
    script = (ROOT/'tooling/build_native_ps5.sh').read_text()
    assert 'BGH V1.1.30' in ui
    assert 'library_v116_container_start_title.bin' in ui
    assert 'branches: [bgh-v1.1.30-fix1]' in workflow
    assert 'bgh-v1.1.30-fix1' in workflow
    assert 'BGH_BawertLs_Game_Hub_V1_1_30' in workflow
    assert 'V1_1_30_${TITLE_ID}_PS5_TEST.zip' in script
