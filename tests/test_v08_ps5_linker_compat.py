"""Regression for GitHub native PS5 libc++ locale link failure in V0.8."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def test_emulator_cover_hash_formatter_needs_no_cpp_locale_streams():
    source = (ROOT / 'native_ps5/emulator_covers.cpp').read_text(encoding='utf-8')
    assert '#include <sstream>' not in source
    assert '#include <iomanip>' not in source
    assert 'std::ostringstream' not in source
    assert 'std::snprintf(digits, sizeof(digits), "%08x", static_cast<unsigned>(value))' in source
