"""V1.1.30 Fix1: guard the deployed PS5 tile against stale hashes."""
from hashlib import sha256
from pathlib import Path
import re
from io import BytesIO
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = (ROOT / '.github/workflows/ps5-build.yml').read_text()
SOURCE = (ROOT / 'sce_sys/icon0.png').read_bytes()


def test_source_checksum_equals_branding_guard_and_both_prior_icon_tests():
    found = re.search(r"expected_icon_sha256\s*=\s*'([0-9a-f]{64})'", WORKFLOW)
    assert found, 'Workflow must pin the approved system icon'
    actual = sha256(SOURCE).hexdigest()
    assert found.group(1) == actual, 'Stale SHA256 causes late PS5 package build failure'
    for test_name in ('test_v1127_ps5_home_icon.py', 'test_v1128_ps5_system_icon.py'):
        assert actual in (ROOT / 'tests' / test_name).read_text()
    with Image.open(BytesIO(SOURCE)) as icon:
        assert icon.format == 'PNG'
        assert icon.size == (512, 512)
        icon.verify()


def test_fix_branch_and_complete_package_guard():
    assert 'branches: [bgh-v1.1.32]' in WORKFLOW
    assert "test \"$GITHUB_REF_NAME\" = 'bgh-v1.1.32'" in WORKFLOW
    assert 'group: bgh-v1.1.32-${{ github.ref }}' in WORKFLOW
    assert 'BGH_V1.1.32_KOMPLETTPROJEKT.zip' in WORKFLOW
    assert "z.read(prefix + 'sce_sys/icon0.png') != source" in WORKFLOW
    assert 'tests/test_v1129_fix1_icon_hash.py' in WORKFLOW
