"""V1.1.14 contract: UI changes only, no reintroduced startup blocker."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
PAD=(ROOT/'native_ps5/controller_status.hpp').read_text()
STORAGE=(ROOT/'native_ps5/storage_widgets.hpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()
BUILD=(ROOT/'tooling/build_native_ps5.sh').read_text()


def between(a,b):
    return UI.split(a,1)[1].split(b,1)[0]


def test_1_recent_title_has_no_extra_line():
    part=between('void draw_recent_cards(', 'void draw_controllers(')
    assert '"Zuletzt gespielt"' in part
    assert 'Letzte Startauftraege' not in part


def test_2_left_section_label_and_count_gone():
    part=between('void draw(app::Frame &frame) const override', 'void request_selected_launch(')
    assert 'section_title' not in part
    assert 'serif_text(list,std::to_string(indices_.size()),72,254' not in part


def test_3_grid_and_emulator_icons_small_and_centered():
    part=between('void draw_platform_icon(', 'int count_games_for_entry(')
    assert 'std::floor(x+1.0f),std::floor(y+6.0f),20,20' in part
    assert '{x-5,y-7,37,37}' not in part.split('if(platform==0 || platform==6)',1)[1]


def test_4_auxiliary_pads_not_polled_in_enter_or_every_frame():
    part=between('void update(const InputFrame &input, float dt, app::Feedback &feedback) override', 'void draw(app::Frame &frame) const override')
    assert 'controllers_.observe_primary(input.connected);' in part
    assert 'if(clock_>=next_controller_poll_)' in part
    assert 'controllers_.poll();' in part
    assert 'next_controller_poll_=clock_+1.0f;' in part
    assert 'next_controller_poll_=5.0f;' in UI
    assert 'sceUserServiceGetInitialUser' in PAD
    assert 'read_pad(initial_user,0,index,index)' in PAD
    assert 'sample+0x4c' in PAD
    assert '0x54' not in PAD


def test_5_storage_status_is_cached_and_icons_exist():
    view=between('void draw_storage_widgets(', 'void draw_detail(')
    assert '::stat(' not in view
    assert 'statvfs(' not in view
    assert 'storage_widgets_.usb_connected' in view
    assert 'shadow_storage_.diagnostic' in UI
    assert 'System bereit' in view
    for filename in ('storage_internal.png','storage_usb.png'):
        assert (ROOT/'assets/themes'/filename).is_file() # Legacy files retained
    # V1.1.20 uses DrawList vector primitives instead of the old bitmap icons.
    assert 'glass_card(list,panel,15);' in view
    assert 'if(!usb_icon)' in view and 'else{' in view
    assert 'list.bordered_rect' in view
    assert '::stat(path,&actual)' in STORAGE
    assert 'out.usb_connected=true;' in STORAGE
    assert 'defined(BGH_VERIFIED_PS5_STATVFS)' in STORAGE


def test_6_counts_match_actual_catalogue_categories():
    part=between('int count_games_for_entry(', 'void draw_storage_widgets(')
    assert 'g.platform!="Homebrew"' in part
    assert 'if(favorites_.count(g.id)>0)++count;' in part
    assert 'if(game_in_recent(g))++count;' in part
    assert 'is_emulator_app(g)' in part
    assert 'g.platform==entry.id' in part
    assert '" | "+count' in part
    assert '0x3B4D68,0.5f' in part


def test_7_snap_fonts_and_correct_version():
    assert 'std::floor(x+1.0f)' in UI
    assert 'std::floor(1814.0f)' in UI
    assert 'std::floor(1861.0f)' in UI
    assert 'context_.fonts.regular,display' in UI
    assert 'BGH V1.1.32' in UI
    assert 'release_label="BGH V1.1.32"' in UI
    assert 'Version: BGH V1.1.32' in UI


def test_release_workflow_builds_integrated_source_without_old_overwrite():
    assert 'branches: [bgh-v1.1.32]' in WF
    assert 'V1_1_32_${TITLE_ID}_PS5_TEST.zip' in BUILD
    assert 'V1_1_32_PPSA99079_PS5_TEST.zip' in WF
    assert 'python3 tooling/bgh_apply_v1110.py' not in WF
    assert 'python3 tooling/bgh_startup_guard_v1111.py' not in WF
    assert 'BGH V1.1.32' in WF
    for needle in ('make_registered_ps5_launch_plan(g.title_id)',
                   'make_registered_ps4_launch_plan(g.title_id)',
                   'stage_ps5sx2_disc_launch(g.path)',
                   'ulg::send_launch(plan)'):
        assert needle in UI


def test_auxiliary_controller_mock(tmp_path):
    exe=tmp_path/'controller-monitor'
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror',
        '-DBGH_TARGET_PS5=1','-I'+str(ROOT/'native_ps5'),
        str(ROOT/'tests/controller_direct_v1117_harness.cpp'),
        '-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
