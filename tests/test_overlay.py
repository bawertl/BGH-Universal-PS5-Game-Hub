from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tooling'))
from install_ui_kit_overlay import apply


def test_single_native_launcher_factory(tmp_path):
    kit=tmp_path/'kit'; concepts=kit/'src'/'concepts';concepts.mkdir(parents=True)
    (kit/'assets').mkdir();(kit/'Makefile').write_text('all:\n\t@true\n')
    (concepts/'concepts.hpp').write_text('namespace hui::concepts {\n} // namespace hui::concepts\n')
    (concepts/'registry.cpp').write_text('ORIGINAL')
    project=Path(__file__).resolve().parents[1]
    apply(project,kit)
    assert (concepts/'library_core.cpp').exists()
    assert (concepts/'universal_launcher.cpp').exists()
    assert (concepts/'registry.cpp.ulg-backup').read_text()=='ORIGINAL'
    assert (concepts/'registry.cpp').read_text().count('make_universal_launcher')==1
    assert (kit/'assets'/'paths.conf').exists()
    apply(project,kit) # rerun is idempotent, backup is preserved
    assert (concepts/'registry.cpp.ulg-backup').read_text()=='ORIGINAL'
    assert (concepts/'concepts.hpp').read_text().count('make_universal_launcher')==1
