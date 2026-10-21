"""V1.0.5: stage recent before all launch handoffs and roll back refused requests."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SRC=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
WORKFLOW=(ROOT/'.github/workflows/ps5-build.yml').read_text()


def test_staging_precedes_all_registered_external_handoffs():
    function=SRC.split('void request_selected_launch(app::Feedback &feedback){',1)[1].split('    // V1.0.5 diagnostics:',1)[0]
    # Native fallback for PS5 shares the first staged history entry.
    assert function.count('record_recent(g.id);') == 5
    assert function.count('rollback_recent(previous_recent);') == 7
    assert function.count('launch_request_accepted_=true;') == 6
    for target in ('ps5_plan','ps4_plan','ps2_plan','plan'):
        send=f'ulg::send_launch({target})'
        pos=function.find(send)
        assert pos!=-1, target
        assert function.rfind('record_recent(g.id);',0,pos)>function.rfind('else if(g.platform==',0,pos),target
    assert 'const auto native=ulg::launch_registered_ps5(g.title_id);' in function
    assert 'if(!launch_request_accepted_)launch_in_progress_=false;' in function
    # We deliberately retain all existing platform-specific calls.
    for code in ('make_registered_ps5_launch_plan(g.title_id)',
                 'make_registered_ps4_launch_plan(g.title_id)',
                 'stage_ps5sx2_disc_launch(g.path)',
                 'make_ps5sx2_menu_launch_plan()',
                 'ulg::send_launch(plan)',
                 'if(bridge.status==ulg::LaunchStatus::queued)sys::quit();'):
        assert code in function


def test_rollback_restores_memory_and_persists_even_empty_backups():
    rollback=SRC.split('void rollback_recent(',1)[1].split('    bool recent_matches(',1)[0]
    assert 'launch_request_accepted_=false;' in rollback
    assert 'recent_list_=previous_recent;' in rollback
    assert 'recent_.clear();' in rollback
    assert '(void)save_prefs();' in rollback
    storage=SRC.split('bool save_prefs(){',1)[1].split('    void record_recent(',1)[0]
    assert 'const bool saved_backup=ulg::write_preferences(backup,recent_path,&recent_status);' in storage
    assert 'backup.recent.empty()' not in storage


def test_prelaunch_seralization_survives_process_restart_and_rollback(tmp_path):
    src=tmp_path/'prelaunch.cpp'
    src.write_text(r'''
#include "state_store.hpp"
#include <cassert>
#include <string>
int main(int argc,char**argv){
  assert(argc==4);
  const std::string action=argv[1];
  const std::string main=argv[2],backup=argv[3];
  const std::string path="PATH|PS5|/user/app/PPSA12345";
  if(action=="stage" || action=="rollback"){
    ulg::Preferences p;
    p.favorites.insert("favourite-old");
    if(action=="stage")p.recent={path};
    assert(ulg::write_preferences(p,main));
    ulg::Preferences b;
    if(action=="stage")b.recent={path};
    assert(ulg::write_preferences(b,backup));
  }else{
    ulg::Preferences p,b;
    assert(ulg::read_preferences(main,p));
    assert(ulg::read_preferences(backup,b));
    assert(p.favorites.count("favourite-old") == 1);
    if(action=="verify-stage"){
      assert(p.recent.size()==1 && p.recent[0]==path);
      assert(b.recent.size()==1 && b.recent[0]==path);
    } else {
      assert(action=="verify-rollback");
      assert(p.recent.empty() && b.recent.empty());
    }
  }
}
''')
    exe=tmp_path/'prelaunch'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',str(ROOT/'native_ps5'),
                    str(src),str(ROOT/'native_ps5/state_store.cpp'),'-o',str(exe)],check=True)
    main,backup=tmp_path/'prefs.bin',tmp_path/'recent.bin'
    for action in ('stage','verify-stage','rollback','verify-rollback'):
        subprocess.run([str(exe),action,str(main),str(backup)],check=True)


def test_consistent_release_markers():
    assert 'branches: [bgh-v1.1.8, main]' in WORKFLOW
    assert '"$GITHUB_REF_NAME" != "bgh-v1.1.8" && "$GITHUB_REF_NAME" != "main"' in WORKFLOW
    assert 'BGH V1.1.8-PLAYTIME-PER-GAME-SAFETY' in SRC
    assert 'BGH-V1-1-8-PLAYTIME-PPSA99079-TEST' in WORKFLOW
    assert 'BGH_BawertLs_Game_Hub_V1_1_8_PPSA99079_PS5_TEST.zip' in WORKFLOW
    assert 'V1_1_8_${TITLE_ID}_PS5_TEST.zip' in (ROOT/'tooling/build_native_ps5.sh').read_text()
