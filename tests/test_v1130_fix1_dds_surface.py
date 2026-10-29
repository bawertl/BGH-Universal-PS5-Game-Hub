"""BGH V1.1.31 Fix1: upstream native kit requires exactly one DDS surface mip."""
from pathlib import Path
import struct
import sys
import pytest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tooling'))
from verify_ps5_home_background import verify_dds_bytes

def test_both_4k_backgrounds_have_single_base_mip_and_identical_payloads():
    a=(ROOT/'sce_sys/pic0.dds').read_bytes()
    b=(ROOT/'sce_sys/pic1.dds').read_bytes()
    assert a==b
    for data in (a,b):
        verify_dds_bytes(data)
        assert struct.unpack_from('<I',data,28)[0]==1
        assert struct.unpack_from('<I',data,128)[0]==98
        assert struct.unpack_from('<I',data,12)[0]==2160
        assert struct.unpack_from('<I',data,16)[0]==3840

def test_old_zero_mip_header_rejected():
    old=bytearray((ROOT/'sce_sys/pic0.dds').read_bytes())
    struct.pack_into('<I',old,28,0)
    with pytest.raises(ValueError,match='offset 28'):
        verify_dds_bytes(old)

def test_encoder_preserves_single_level_profile_and_workflow_covers_it():
    source=(ROOT/'tooling/encode_ps5_bc7_mode6.cpp').read_text()
    wf=(ROOT/'.github/workflows/ps5-build.yml').read_text()
    assert 'le32(out,W*H);le32(out,0);le32(out,1);' in source
    assert 'tests/test_v1130_fix1_dds_surface.py' in wf
    assert "test \"$GITHUB_REF_NAME\" = 'bgh-v1.1.31-fix1'" in wf
