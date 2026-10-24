#!/usr/bin/env python3
"""BGH V1.1.11: fail-closed startup stabilization of the V1.1.10 build-time UI patch.

Run AFTER bgh_apply_v1110.py, against the checked-out project before compiling.
Do not touch emulator scanners, launch bridges, game data, or user preferences.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / 'native_ps5/universal_launcher.cpp'
PAD = ROOT / 'native_ps5/controller_status.hpp'
BUILD = ROOT / 'tooling/build_native_ps5.sh'


def once(s: str, old: str, new: str, what: str) -> str:
    count = s.count(old)
    if count != 1:
        raise RuntimeError(f'{what}: expected once, found {count}')
    return s.replace(old, new, 1)


def function_bounds(s: str, name: str):
    match = re.search(r'\bvoid\s+' + re.escape(name) + r'\s*\(', s)
    if not match:
        raise RuntimeError(f'Function missing: {name}')
    brace = s.find('{', match.end())
    if brace == -1:
        raise RuntimeError(f'Function missing body: {name}')
    depth = 0
    for n in range(brace, len(s)):
        if s[n] == '{':
            depth += 1
        elif s[n] == '}':
            depth -= 1
            if depth == 0:
                return match.start(), n + 1
    raise RuntimeError(f'Unbalanced braces in {name}')


def patch():
    old = CPP.read_text(encoding='utf-8')
    if 'BGH V1.1.10-NEXTGEN-UI-POLISH' not in old:
        raise RuntimeError('Source is not post-V1.1.10-patch; do not modify.')
    s = old
    s = once(s, '#include "bgh_storage_metrics.hpp"\n', '', 'Remove statvfs header from launcher')
    s = once(s, '    mutable ulg::bgh_storage::Volume gallery_volumes_[2];\n', '',
             'Remove unused storage widget state')
    s = once(s, '    mutable float gallery_next_storage_poll_=-1.0f;\n', '',
             'Remove unsafe storage polling state')

    a, b = function_bounds(s, 'draw_storage_widgets')
    widget = s[a:b]
    # The V1.1.10 storage overlay was appended immediately before the closing
    # brace. Remove ONLY that exact appended block, leaving original widgets.
    key = '        if(gallery_next_storage_poll_<0.0f || clock_>=gallery_next_storage_poll_){'
    if widget.count(key) != 1:
        raise RuntimeError('Unexpected storage renderer; refusing broad deletion')
    start = widget.index(key)
    overlay = widget[start:-1]
    for required in ('ulg::bgh_storage::internal()', 'ulg::bgh_storage::external()',
                     'gallery_volumes_', 'gallery_next_storage_poll_'):
        if required not in overlay:
            raise RuntimeError('Missing expected storage overlay marker: ' + required)
    # Everything after the marker must be the known V1.1.10 overlay.
    if 'for(int i=0;i<2;++i)' not in overlay:
        raise RuntimeError('Storage overlay signature changed')
    s = s[:a] + widget[:start] + '}' + s[b:]

    # V1.1.10's new secondary-controller handle probing is not verified on this
    # firmware. Disable all optional direct Pad polling on startup. The UI kit's
    # primary InputFrame -> observe_primary() still reports controller 1.
    s = once(s,
        'if(clock_>=next_controller_poll_){controllers_.poll();next_controller_poll_=clock_+1.0f;}',
        '/* V1.1.11 safe mode: optional scePad monitor disabled until PS5 verification. */',
        'Disable optional Pad polling')
    if 'controllers_.observe_primary(input.connected);' not in s:
        raise RuntimeError('Primary UI-kit controller status missing')

    # Do not enumerate user FTP theme folders or open gallery.cfg during the
    # UniversalLauncher constructor, before the renderer has entered.
    ctor = '''        if(!ulg::bgh_gallery::load_selection("/download0/hui/ulg_v04/gallery.cfg",gallery_selection_))
            (void)ulg::bgh_gallery::load_selection("/data/bgh/gallery.cfg",gallery_selection_);
        refresh_gallery_paths();
'''
    s = once(s, ctor, '', 'Defer gallery config reads until settings requested')
    s = once(s,
        'ulg::bgh_gallery::Selection gallery_selection_{"mountain_crown","none"};',
        'ulg::bgh_gallery::Selection gallery_selection_{"lake","legacy"};',
        'Conservative fallback theme')
    s = once(s,
        'std::string gallery_background_cache_="/app0/assets/themes/backgrounds/bgh_berg_cyber_mit_kronenlogo.png";',
        'std::string gallery_background_cache_="/app0/assets/premium_lake_background.png";',
        'Known pre-V1.1.10 default background')
    s = once(s,
        'std::string gallery_logo_cache_;',
        'std::string gallery_logo_cache_="/app0/assets/bgh_logo.png";\n    bool gallery_selection_loaded_after_boot_=false;',
        'Known bundled logo and deferred-load flag')
    # Attempt saved theme selection only when user explicitly visits the theme
    # settings tab. All image decodes remain in update() with GL context alive.
    s = once(s, '        clock_ += dt;', '''        clock_ += dt;
        if(settings_ && settings_tab_==3 && !gallery_selection_loaded_after_boot_){
            gallery_selection_loaded_after_boot_=true;
            if(!ulg::bgh_gallery::load_selection("/download0/hui/ulg_v04/gallery.cfg",gallery_selection_))
                (void)ulg::bgh_gallery::load_selection("/data/bgh/gallery.cfg",gallery_selection_);
            refresh_gallery_paths();
        }''', 'Deferred gallery load after first renderer update')
    s = once(s,
        'const auto new_background=gallery_background_path(); if(!covers_.has(new_background))covers_.load(new_background);',
        'const auto new_background=gallery_background_path(); if(!new_background.empty() && !covers_.has(new_background))covers_.load(new_background);',
        'Safe nonempty background')
    # Existing logo load already checks !new_logo.empty(); CoverTextures::get
    # returns 0 for an absent texture and drawing uses if(image) guards.
    if 'if(!new_logo.empty() && !covers_.has(new_logo))covers_.load(new_logo);' not in s:
        raise RuntimeError('Logo texture guard missing')
    if re.search(r'\b(?:statvfs\s*\(|bgh_storage::internal\s*\(|bgh_storage::external\s*\(|controllers_\.poll\s*\()', s):
        raise RuntimeError('Forbidden startup API call remains in launcher')
    s = s.replace('BGH V1.1.10-NEXTGEN-UI-POLISH', 'BGH V1.1.11-STARTUP-HOTFIX')
    s = s.replace('release_label="BGH V1.1.10"', 'release_label="BGH V1.1.11"')
    s = s.replace('BGH V1.1.10', 'BGH V1.1.11')

    pad = PAD.read_text(encoding='utf-8')
    start = '        // Try additional pad ports for guests sharing the primary login.\n'
    end = '\n#endif\n    }\nprivate:'
    if start in pad:
        p = pad.index(start)
        q = pad.find(end, p)
        if q == -1:
            raise RuntimeError('Unexpected unsafe second-controller code')
        pad = pad[:p] + pad[q:]
    # Keep existing controller status backend for later investigation, without
    # calling it from the UI. No dlsym or Pad call in constructors.
    if 'ControllerStatus()=default;' not in pad or 'void poll(){' not in pad:
        raise RuntimeError('Unexpected controller ABI; refusing change')

    sh = BUILD.read_text(encoding='utf-8')
    if 'BGH_BawertLs_Game_Hub_V1_1_10_${TITLE_ID}_PS5_TEST.zip' not in sh:
        raise RuntimeError('Expected V1.1.10 PS5 package name missing')
    sh = sh.replace('V1_1_10', 'V1_1_11').replace('V1.1.10', 'V1.1.11')
    # Compute everything before committing, so any guard failure changes nothing.
    CPP.write_text(s, encoding='utf-8')
    PAD.write_text(pad, encoding='utf-8')
    BUILD.write_text(sh, encoding='utf-8')
    print('BGH V1.1.11 startup guard applied: storage probes + optional Pad polling OFF; UI-kit primary ON.')


if __name__ == '__main__':
    try:
        patch()
    except Exception as e:
        print('BGH V1.1.11 STARTUP GUARD FAILED: ' + str(e), file=sys.stderr)
        sys.exit(1)
