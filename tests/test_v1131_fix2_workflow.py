"""Guard the actual release branch against stale backup assertions."""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
WF = (ROOT / '.github/workflows/ps5-build.yml').read_text()

def test_branch_and_backup_consistent():
    assert 'branches: [bgh-v1.1.31-fix2]' in WF
    assert 'test "$GITHUB_REF_NAME" = \'bgh-v1.1.31-fix2\'' in WF
    assert 'build/backup/BGH_V1.1.31_FIX2/' in WF
    assert 'BGH_V1.1.31_FIX2_KOMPLETTPROJEKT.zip BGH_V1.1.31_FIX2)' in WF
    assert 'name: BGH-V1-1-31-FIX2-KOMPLETTPROJEKT' in WF
    assert 'name: BGH-V1-1-31-FIX2-PPSA99079' in WF
    assert 'BGH V1.1.31' in (ROOT / 'native_ps5/universal_launcher.cpp').read_text()

def test_tests_do_not_expect_unversioned_backup():
    for name in ('test_v1125_background_grid.py','test_v1126_fix1_workflow.py','test_v1129_fix1_icon_hash.py'):
        txt = (ROOT / 'tests'/name).read_text()
        assert 'BGH_V1.1.31_KOMPLETTPROJEKT.zip' not in txt
        assert 'BGH_V1.1.31/' not in txt
