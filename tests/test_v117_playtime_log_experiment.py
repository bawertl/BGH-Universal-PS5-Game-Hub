"""V1.1.8: log-only PS5 experiment; never modify proven launch paths."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]


def test_cpp_parser_rollover_sleep_duplicates_uncertainty(tmp_path):
    cli=tmp_path/'playtime_host'
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',
        str(ROOT/'native_ps5/playtime_test_cli.cpp'),'-o',str(cli)],
        check=True,timeout=60,capture_output=True,text=True)
    run=subprocess.run([str(cli)],capture_output=True,text=True,check=True,timeout=5)
    assert 'Playtime host tests OK' in run.stdout


def test_log_only_on_details_and_no_start_route_changes():
    text=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
    assert 'refresh_playtime_log();detail_=true' in text
    assert 'ulg::playtime::read_local_log("/data/shadowmount/debug.log",bytes)' in text
    assert 'ftp.retrieve(' in text
    assert 'Log-Spielzeit:' in text
    assert 'Emulator-Laufzeit (Log):' not in text
    assert 'ROM-Spielzeit noch nicht messbar' in text
    assert 'native_log_time_label(g)' in text
    assert 'is_native_game_id(g.platform,g.title_id)' in text
    assert 'make_registered_ps5_launch_plan(g.title_id)' in text
    assert 'stage_ps5sx2_disc_launch(g.path)' in text
    assert 'sys::quit()' in text
    header=(ROOT/'native_ps5/playtime_log.hpp').read_text()
    assert 'fopen(path,"rb")' in header
    assert 'kMaxLogBytes=2*1024*1024' in header
    assert 'finalizing stopped application:' in header
    assert '[SLEEP] entered' in header
    assert 'active.erase(it);' in header
    assert 'fopen(path,"wb")' not in header
