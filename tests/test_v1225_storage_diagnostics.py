"""V1.0.5: recent storage fix with direct probes and /data fallback."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SOURCE=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
WORKFLOW=(ROOT/'.github/workflows/ps5-build.yml').read_text()


def test_real_cxx_storage_status_errno_and_atomic_roundtrip(tmp_path):
    cpp=tmp_path/'probe.cpp'
    cpp.write_text(r'''
#include "state_store.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <string>
#include <filesystem>
int main(int argc, char **argv){
    assert(argc==2);
    const std::string root=argv[1];
    const std::string path=root+"/preferences.bin";
    ulg::StorageStatus s;
    ulg::Preferences p;
    assert(!ulg::read_preferences(path,p,&s));
    assert(!s.ok && s.stage=="open" && s.os_error==ENOENT);
    ulg::Preferences prefs;
    prefs.favorites.insert("FAVORITE");
    prefs.recent.push_back("PATH|PS5|/user/app/PPSA77777");
    assert(!ulg::write_preferences(prefs,root+"/missing/preferences.bin",&s));
    assert(!s.ok && s.stage=="open_tmp" && s.os_error==ENOENT);
    assert(ulg::write_preferences(prefs,path,&s));
    assert(s.ok && s.stage=="saved" && s.os_error==0);
    ulg::Preferences loaded;
    assert(ulg::read_preferences(path,loaded,&s));
    assert(s.ok && s.stage=="loaded" && loaded.recent==prefs.recent);
    assert(loaded.favorites==prefs.favorites);
    FILE *f=std::fopen(path.c_str(),"ab"); assert(f);
    assert(std::fwrite("X",1,1,f)==1);assert(std::fclose(f)==0);
    assert(!ulg::read_preferences(path,loaded,&s));
    assert(!s.ok && s.stage=="checksum" && s.os_error==0);
    const std::string directory=root+"/occupied";
    std::filesystem::create_directory(directory);
    assert(!ulg::write_preferences(prefs,directory,&s));
    assert(!s.ok && s.stage=="rename" && s.os_error!=0);
    assert(std::filesystem::is_directory(directory));
    assert(!std::filesystem::exists(directory+".tmp"));
}
''')
    exe=tmp_path/'storage_diagnostics'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
                    '-I',str(ROOT/'native_ps5'),str(cpp),str(ROOT/'native_ps5/state_store.cpp'),
                    '-o',str(exe)],check=True)
    subprocess.run([str(exe),str(tmp_path)],check=True)


def test_storage_fix_uses_dual_read_dual_write_and_direct_probes():
    assert 'read_preferences(kPrimaryPrefsFile,primary_preferences,&prefs_read_status_)' in SOURCE
    assert 'read_preferences(kFallbackPrefsFile,fallback_preferences,&fallback_prefs_read_status_)' in SOURCE
    assert 'read_preferences(kPrimaryRecentBackupFile,recent_backup,&recent_read_status_)' in SOURCE
    assert 'read_preferences(kFallbackRecentBackupFile,fallback_recent_backup,&fallback_recent_read_status_)' in SOURCE
    assert 'save_location(kPrimaryPrefsFile,kPrimaryRecentBackupFile' in SOURCE
    assert 'save_location(kFallbackPrefsFile,kFallbackRecentBackupFile' in SOURCE
    probe=SOURCE.split('void probe_recent_storage(){',1)[1].split('    static std::string storage_status_label',1)[0]
    assert 'storage_probe_v126_primary.bin' in probe
    assert 'storage_probe_v126_fallback.bin' in probe
    assert 'run_storage_probe(' in SOURCE
    assert 'directory_unavailable' not in probe
    assert '"/data/bgh"' in SOURCE
    assert 'VERLAUF-DIAG  PRIMARY=' in SOURCE


def test_original_five_platform_launch_block_keeps_all_core_launch_paths():
    launch = SOURCE.split('void request_selected_launch(app::Feedback &feedback){',1)[1].split('    // V1.0.5 diagnostics:',1)[0]
    assert launch.count('record_recent(g.id);') == 4
    for token in (
        'make_registered_ps5_launch_plan(g.title_id)',
        'make_registered_ps4_launch_plan(g.title_id)',
        'stage_ps5sx2_disc_launch(g.path)',
        'make_ps5sx2_menu_launch_plan()',
        'ulg::send_launch(ps5_plan)',
        'ulg::send_launch(ps4_plan)',
        'ulg::send_launch(plan)',
        'ulg::launch_registered_ps5(g.title_id)',
        'if(bridge.status==ulg::LaunchStatus::queued)sys::quit();'):
        assert token in launch


def test_workflow_is_v126_and_legacy_launch_checks_remain():
    assert 'branches: [bgh-v1.1.5, main]' in WORKFLOW
    assert '"$GITHUB_REF_NAME" != "bgh-v1.1.5" && "$GITHUB_REF_NAME" != "main"' in WORKFLOW
    assert 'BGH-V1-1-5-COVER-PPSA99079-TEST' in WORKFLOW
    assert 'V1_1_5_PPSA99079_PS5_TEST.zip' in WORKFLOW
    assert 'BGH V1.1.5-CONTAINER-COVER-FIX' in SOURCE
    for token in ('stage_ps5sx2_disc_launch(g.path)',
                  'make_registered_ps5_launch_plan(g.title_id)',
                  'make_registered_ps4_launch_plan(g.title_id)',
                  'make_ps5sx2_menu_launch_plan()',
                  'if(bridge.status==ulg::LaunchStatus::queued)sys::quit();'):
        assert token in SOURCE
