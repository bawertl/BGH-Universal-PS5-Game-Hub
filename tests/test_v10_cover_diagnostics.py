from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def test_diagnosis_available_in_detail():
    text=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
    assert 'COVER-DIAGNOSE:' in text and 'cover_diagnostics_[g.id]' in text
    assert 'attempted_emulator_covers_.clear();cover_diagnostics_.clear();' in text
def test_cover_importer_reports_results():
    text=(ROOT/'native_ps5/emulator_covers.cpp').read_text()
    assert 'PS2: SERIENNUMMER' in text
    assert 'SWITCH: HASH GEFUNDEN' in text
    assert 'XBOX 360: TITEL-ID' in text
    assert 'FTP-ABRUF:' in text
    assert 'BILDSIGNATUR UNGUELTIG' in text
    assert 'BGH-CACHE-ORDNER NICHT VERFUEGBAR' in text
    assert 'BILD-DEKODIERUNG:' in text
    assert 'BILD GEFUNDEN, ABER NICHT LESBAR / DEKODIERBAR' not in text
