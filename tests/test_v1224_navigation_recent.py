"""V1.0.5: direct launch, fallback recent storage and banner sizing."""
import subprocess
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT/'native_ps5/universal_launcher.cpp').read_text()
WORKFLOW = (ROOT/'.github/workflows/ps5-build.yml').read_text()


def test_direct_launch_and_l3_details_keep_existing_start_paths():
    update = SRC.split('void update(const InputFrame &input, float dt, app::Feedback &feedback) override {',1)[1].split('void draw(app::Frame &frame) const override',1)[0]
    launch = SRC.split('void request_selected_launch(app::Feedback &feedback){',1)[1].split('    // V1.0.5 diagnostics:',1)[0]
    assert 'if(input.is_pressed(Action::confirm) && !sidebar_focus_)request_selected_launch(feedback);' in update
    assert 'if(input.is_pressed(Action::l3) && !sidebar_focus_ && !indices_.empty())' in update
    assert 'if(input.is_pressed(Action::page_prev)){' in update
    assert 'if(input.is_pressed(Action::page_next)){' in update
    assert 'switch_tab((tab_+static_cast<int>(sidebar_items_.size())-1)%static_cast<int>(sidebar_items_.size()));' in update
    assert 'if(input.is_pressed(Action::l3) || input.is_pressed(Action::confirm))' in update
    assert 'focus_recent_game(recent[recent_selected_])' in update
    assert 'request_selected_launch(feedback)' in update
    assert 'if(indices_.empty() || launch_in_progress_ || clock_<launch_retry_after_)return;' in launch
    assert 'if(!launch_request_accepted_)launch_in_progress_=false;' in launch
    assert launch.count('record_recent(g.id);') == 5
    for api in ('make_registered_ps5_launch_plan(g.title_id)',
                'make_registered_ps4_launch_plan(g.title_id)',
                'stage_ps5sx2_disc_launch(g.path)',
                'ulg::send_launch(plan)',
                'ulg::launch_registered_ps5(g.title_id)'):
        assert api in launch


def test_fallback_recent_storage_and_ui_guardrails_are_present():
    assert 'const std::string stable="PATH|"+game->platform+"|"+game->path;' in SRC
    assert 'save_location(kPrimaryPrefsFile,kPrimaryRecentBackupFile' in SRC
    assert 'save_location(kFallbackPrefsFile,kFallbackRecentBackupFile' in SRC
    assert 'read_preferences(kFallbackPrefsFile' in SRC
    assert 'read_preferences(kFallbackRecentBackupFile' in SRC
    assert 'run_storage_probe(kPrimaryStateFolder,"storage_probe_v126_primary.bin"' in SRC
    assert 'run_storage_probe(kFallbackStateFolder,"storage_probe_v126_fallback.bin"' in SRC
    assert 'Startaufträge – kein Nachweis der Spiel-Laufzeit' in SRC
    assert 'if(!launch_message_.empty())serif_text(list,shorten(launch_message_,93)' in SRC


def test_header_png_assets_are_exact_900x220_rgba():
    for key in ('switch','ps5','ps4'):
        with Image.open(ROOT/f'assets/ui/fan_case_header_{key}.png') as im:
            assert im.mode == 'RGBA'
            assert im.size == (900,220)
            a = im.getchannel('A').getextrema()
            assert a[1] > 0


def test_real_preferences_persist_path_based_recent_and_reload(tmp_path):
    program = tmp_path/'recent_roundtrip.cpp'
    program.write_text(r'''
#include "state_store.hpp"
#include <cassert>
#include <string>
int main(int argc,char **argv){
    assert(argc==2);
    ulg::Preferences expected;
    expected.favorites.insert("fav:Mario");
    expected.recent={"PATH|Switch|/mnt/usb1/Switch/Mario Kart 8 Deluxe.nsp",
                     "PATH|PS2|/mnt/usb1/PS5SX2/games/obsCure.chd"};
    assert(ulg::write_preferences(expected,argv[1]));
    ulg::Preferences fresh;
    assert(ulg::read_preferences(argv[1],fresh));
    assert(fresh.recent==expected.recent);
    assert(fresh.favorites==expected.favorites);
    const std::string replay=expected.recent.at(1);
    fresh.recent.erase(fresh.recent.begin()+1);
    fresh.recent.insert(fresh.recent.begin(),replay);
    assert(ulg::write_preferences(fresh,argv[1]));
    ulg::Preferences again;
    assert(ulg::read_preferences(argv[1],again));
    assert(again.recent.at(0)==replay);
    assert(again.recent.size()==2);
}
''')
    exe = tmp_path/'prefs'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
                    '-I',str(ROOT/'native_ps5'),str(program),
                    str(ROOT/'native_ps5/state_store.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(tmp_path/'preferences.bin')],check=True)


def test_workflow_branch_artifact_and_native_package_paths():
    build = (ROOT/'tooling/build_native_ps5.sh').read_text()
    assert 'branches: [bgh-v1.1.9, main]' in WORKFLOW
    assert 'BGH-V1-1-9-EMULATORS-PPSA99079-TEST' in WORKFLOW
    assert 'V1_1_9_PPSA99079_PS5_TEST.zip' in WORKFLOW
    assert 'V1_1_9_${TITLE_ID}_PS5_TEST.zip' in build
    assert '"$GITHUB_REF_NAME" != "bgh-v1.1.9" && "$GITHUB_REF_NAME" != "main"' in WORKFLOW


def test_real_ui_kit_l3_action_uses_supported_left_stick_glyph():
    # PS5 UI kit (pinned bbe1905): Action::l3 exists, ui::Button::l3 does not.
    assert 'Action::l3' in SRC
    assert '{ui::Button::left_stick,hint("Details")}' in SRC
    assert 'ui::Button::l3' not in SRC
    shim = (ROOT/'tooling/host_shim/concepts/concepts.hpp').read_text()
    assert 'l3' in shim and 'left_stick' in shim
