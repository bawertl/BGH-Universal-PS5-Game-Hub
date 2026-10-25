"""Regression guards for V0.12.5; device-specific permissions remain PS5-only."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / 'native_ps5/universal_launcher.cpp').read_text()
IMP = (ROOT / 'native_ps5/emulator_covers.cpp').read_text()
FTP = (ROOT / 'native_ps5/ftp_readonly.cpp').read_text()


def test_cache_directory_failure_cannot_prevent_emulator_lookup():
    assert 'prepare_directory("/download0/hui/ulg_v04/covers")' in UI
    assert 'prepare_directory(kEmulatorCoverFolder)' in UI
    assert 'if(!cover_directory_ready_)' not in UI
    # This guard concerns cover/cache setup only. V1.0.5 intentionally
    # uses opendir(folder) in a separate, observational storage diagnostic.
    directory_setup = UI.split('const auto prepare_directory=', 1)[1].split('refresh_storage_directory_status();', 1)[0]
    assert 'opendir(folder)' not in directory_setup
    assert 'lstat(dir.c_str()' not in IMP
    assert 'CACHE-DATEI OEFFNEN' in IMP
    assert 'read_private_cache(path,bytes)' in IMP


def test_native_ps5_ftp_uses_legacy_retr_without_size():
    old = FTP.split('bool FtpReadonly::retrieve(', 1)[1].split('FtpTransferStatus FtpReadonly::retrieve_detailed', 1)[0]
    assert 'command("RETR "+path,reply)' in old
    assert 'SIZE ' not in old
    assert 'if(code==0 || code==421)disconnect()' in old
    assert 'const int completion=response(reply)' in old
    detailed = FTP.split('FtpTransferStatus FtpReadonly::retrieve_detailed',1)[1]
    assert 'FTP SIZE TIMEOUT/VERBINDUNG VERLOREN' in detailed
    assert 'bytes.clear();disconnect();return status' in detailed


def test_gpu_failure_never_deletes_emulator_image():
    assert 'if(covers_.decode_failed(failed_path)' in UI
    assert 'glGetError()' in UI
    assert 'GPU/GL UPLOAD FEHLER' in UI
    assert 'BILD-DEKODIERUNG:' in UI


def test_visible_build_marker_and_package_name():
    assert 'BGH V1.1.27' in UI
    workflow = (ROOT / '.github/workflows/ps5-build.yml').read_text()
    assert 'bgh-v1.1.27' in workflow
    assert 'V1_1_27_PPSA99079_PS5_TEST.zip' in workflow
    assert 'V1_1_27_' in (ROOT / 'tooling/build_native_ps5.sh').read_text()
