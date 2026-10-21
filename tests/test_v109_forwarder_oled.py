"""V1.0.9: verified menu-only packets and visual-theme regression guards."""
from pathlib import Path
import socket
import struct
import subprocess
import threading

ROOT=Path(__file__).resolve().parents[1]
NATIVE=ROOT/'native_ps5'
UI=(NATIVE/'universal_launcher.cpp').read_text()
CLIENT=(NATIVE/'launcher_client.cpp').read_text()
HEADER=(NATIVE/'launcher_client.hpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()


def segment(begin,end):
    return UI.split(begin,1)[1].split(end,1)[0]


def test_real_homebrew_entries_have_forwarder_menu_plans():
    assert 'LaunchPlan make_prosperoeden_menu_launch_plan();' in HEADER
    assert 'LaunchPlan make_ps5x360_menu_launch_plan();' in HEADER
    for func,identifier in [('make_prosperoeden_menu_launch_plan','PPSA99008'),
                            ('make_ps5x360_menu_launch_plan','PPSA50011')]:
        body=CLIENT.split('LaunchPlan '+func+'(){',1)[1].split('return p;',1)[0]
        assert 'p.target="'+identifier+'";' in body
        assert 'p.supported=true;' in body
        assert 'p.argv=' not in body
    assert 'p.target="PPSA99203";' in CLIENT


def test_menu_actions_use_forwarder_without_touching_recent():
    launch=segment('void request_selected_launch(', 'void refresh_storage(')
    homebrew=launch.split('if(selected_game.platform=="Homebrew")',1)[1].split('// Menu-only mode never stages',1)[0]
    menu_mode=launch.split('if(!emulator_boot_settings_.direct_boot &&',1)[1].split('launch_in_progress_=true;',1)[0]
    for block in [homebrew,menu_mode]:
        assert 'ulg::send_launch(emulator_menu_plan(' in block
        assert 'ulg::launch_emulator_menu(' not in block
        assert 'record_recent(' not in block
        assert 'stage_ps5sx2_disc_launch(' not in block
        assert 'sys::quit()' in block
    assert 'if(!selected_game.available)' not in menu_mode
    detail=segment('if(detail_){','// Touchpad gives focus')
    assert 'ulg::send_launch(emulator_menu_plan(g.platform))' in detail
    assert 'ulg::launch_emulator_menu(' not in detail
    for existing in ('stage_ps5sx2_disc_launch(g.path)',
                     'make_registered_ps4_launch_plan(g.title_id)',
                     'make_registered_ps5_launch_plan(g.title_id)',
                     'p.target="PPSA99008";p.argv={"--rom",game_path,"--exit-after-game"}'):
        assert existing in UI+CLIENT


def test_forwarder_serializes_zero_rom_arguments_for_both_emulators(tmp_path):
    cli=tmp_path/'launch-test-cli'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
        '-Inative_ps5',str(NATIVE/'launch_test_cli.cpp'),str(NATIVE/'launcher_client.cpp'),
        '-o',str(cli)],cwd=ROOT,check=True,capture_output=True,timeout=60)
    for name,identifier in [('SWITCHMENU',b'PPSA99008'),('XBOXMENU',b'PPSA50011')]:
        with socket.socket() as server:
            server.bind(('127.0.0.1',0));server.listen(1);server.settimeout(5)
            port=server.getsockname()[1]
            captured=[]
            def serve():
                conn,_=server.accept()
                with conn:
                    data=bytearray()
                    while len(data)<0x1020:
                        part=conn.recv(0x1020-len(data))
                        if not part:break
                        data.extend(part)
                    captured.append(bytes(data))
                    conn.sendall(struct.pack('<6I',0x4C574650,1,3,0x80940010,0x80940010,0))
            thread=threading.Thread(target=serve,daemon=True);thread.start()
            run=subprocess.run([str(cli),name,'not-a-rom',str(port)],capture_output=True,text=True,timeout=6)
            thread.join(timeout=5)
            assert run.returncode==0 and run.stdout.startswith('1|'),run.stdout
            assert len(captured)==1
            packet=captured[0]
            assert len(packet)==0x1020
            assert packet[8:17]==identifier
            assert struct.unpack_from('<II',packet,0x18)==(0,0)
            assert packet[0x20:]==b'\x00'*4096


def test_oled_shadows_and_teal_glass_have_selected_only_glow():
    fan=segment('void draw_fan_cover(', 'void draw_cover_fan(')
    for marker in ('0x02060C,0.55f','water_y=card.y+card.h-6.0f',
                   '0x010307,shadow_alpha','0x020509,shadow_alpha*0.75f',
                   '0x060B12','if(depth<0.15f)'):
        assert marker in fan
    assert fan.count('list.glow(')==1
    glass=segment('void glass_card(', 'void draw_recent_cards(')
    assert '0x0D1F2A,0.36f' in glass
    assert '0x08151E,0.46f' in glass
    assert '0xA8D0D6,0.22f' in glass
    game_list=segment('void draw_game_list(', 'void draw_fan_cover(')
    assert '0x7CE0C8,0.65f' in game_list
    nav=segment('void draw_platform_bar(', 'void draw_detail(')
    assert '0x7CE0C8,0.70f' in nav


def test_version_ci_identity_is_consistent():
    assert 'BGH V1.1.8-PLAYTIME-PER-GAME-SAFETY' in UI
    assert 'release_label="BGH V1.1.8"' in UI
    assert 'branches: [bgh-v1.1.8, main]' in WF
    assert 'BGH-V1-1-8-PLAYTIME-PPSA99079-TEST' in WF
    assert 'V1_1_8_${TITLE_ID}_PS5_TEST.zip' in (ROOT/'tooling/build_native_ps5.sh').read_text()
