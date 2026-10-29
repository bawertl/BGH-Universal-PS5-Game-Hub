"""BGH V1.1.31 Fix1: incomplete custom-emulator protocol warning."""
from pathlib import Path
S=(Path(__file__).resolve().parents[1]/'native_ps5/universal_launcher.cpp').read_text()
W=(Path(__file__).resolve().parents[1]/'.github/workflows/ps5-build.yml').read_text()

def test_warning_is_contextual_and_not_a_global_block():
    assert 'emulator_protocol_needs_confirmation' in S
    assert 'p.launch_protocol!="auto"' in S
    assert 'p.app_path.empty()' in S
    assert 'id!="PPSA99008"' in S
    assert 'has_unconfirmed_emulator_setup()' in S
    assert 'if(emulator_setup_warning_open_)' in S
    assert 'draw_emulator_setup_warning(frame.scene)' in S
    assert '!!! STARTMETHODE FEHLT - DREIECK !!!' in S
    assert 'test_v1131_fix1_emulator_warning.py' in W

def test_existing_emulator_protocol_is_preserved():
    assert 'target.launch_protocol=modes[(n+1)%6]' in S
    assert 'persist_platform_registry(std::move(changed))' in S
    assert 'configured_rom_launch' in S
