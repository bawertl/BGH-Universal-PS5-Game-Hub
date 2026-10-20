"""V1.0.5 PS5 link guard: time conversion must not import localtime_r."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'native_ps5' / 'universal_launcher.cpp'


def test_ps5_time_conversion_imports_supported_sdk_symbol(tmp_path):
    source = SOURCE.read_text()
    assert 'extern "C" std::tm *localtime_s(const std::time_t *, std::tm *);' in source
    assert 'if(localtime_s(&now,&local_time)==nullptr)' in source
    assert 'if(localtime_r(&now,&local_time)==nullptr)' in source  # Linux only

    obj = tmp_path / 'universal_launcher_ps5_path.o'
    subprocess.run([
        'g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
        '-U__linux__', '-c', '-I' + str(ROOT / 'tooling' / 'host_shim'),
        '-I' + str(ROOT / 'native_ps5'), str(SOURCE), '-o', str(obj),
    ], check=True, timeout=45)
    symbols = subprocess.check_output(['nm', '-u', str(obj)], text=True, timeout=10)
    assert 'localtime_s' in symbols, symbols
    assert 'localtime_r' not in symbols, symbols
