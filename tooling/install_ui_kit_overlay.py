#!/usr/bin/env python3
"""Copy ONLY these sources into a local PS5 Homebrew UI kit checkout.

Does not download dependencies, build, deploy, touch PS5, or run emulator code.
Never overwrites existing native UI kit files except its known factory list and registry
(after writing .ulg-backup sibling files).
"""
from pathlib import Path
import argparse
import shutil


def apply(project: Path, toolkit: Path) -> None:
    concepts = toolkit / 'src' / 'concepts'
    header = concepts / 'concepts.hpp'
    registry = concepts / 'registry.cpp'
    if not (header.is_file() and registry.is_file() and (toolkit/'Makefile').is_file()):
        raise SystemExit('Fehler: Kein gültiger Checkout von blackbearreloaded/ps5-homebrew-ui.')
    for path in (header, registry):
        backup = path.with_name(path.name + '.ulg-backup')
        if not backup.exists():
            shutil.copy2(path, backup)
    shutil.copy2(project/'native_ps5'/'universal_launcher.cpp', concepts/'universal_launcher.cpp')
    shutil.copy2(project/'native_ps5'/'premium_navigation.hpp', concepts/'premium_navigation.hpp')
    shutil.copy2(project/'native_ps5'/'cover_load_debounce.hpp', concepts/'cover_load_debounce.hpp')
    shutil.copy2(project/'native_ps5'/'ui_localization.hpp', concepts/'ui_localization.hpp')
    shutil.copy2(project/'native_ps5'/'serif_atlas_metrics.hpp', concepts/'serif_atlas_metrics.hpp')
    shutil.copy2(project/'native_ps5'/'controller_status.hpp', concepts/'controller_status.hpp')
    shutil.copy2(project/'native_ps5'/'storage_widgets.hpp', concepts/'storage_widgets.hpp')
    shutil.copy2(project/'native_ps5'/'storage_shadowmount.hpp', concepts/'storage_shadowmount.hpp')
    shutil.copy2(project/'native_ps5'/'playtime_log.hpp', concepts/'playtime_log.hpp')
    shutil.copy2(project/'native_ps5'/'platform_registry.hpp', concepts/'platform_registry.hpp')
    shutil.copy2(project/'native_ps5'/'emulator_detection.hpp', concepts/'emulator_detection.hpp')
    shutil.copy2(project/'native_ps5'/'emulator_cards.hpp', concepts/'emulator_cards.hpp')
    shutil.copy2(project/'native_ps5'/'library_core.hpp', concepts/'library_core.hpp')
    shutil.copy2(project/'native_ps5'/'library_core.cpp', concepts/'library_core.cpp')
    shutil.copy2(project/'native_ps5'/'launcher_client.cpp', concepts/'launcher_client.cpp')
    shutil.copy2(project/'native_ps5'/'launcher_client.hpp', concepts/'launcher_client.hpp')
    shutil.copy2(project/'native_ps5'/'ps2_autostart.hpp', concepts/'ps2_autostart.hpp')
    shutil.copy2(project/'native_ps5'/'ftp_ps2_writer.hpp', concepts/'ftp_ps2_writer.hpp')
    shutil.copy2(project/'native_ps5'/'ftp_ps2_writer.cpp', concepts/'ftp_ps2_writer.cpp')
    shutil.copy2(project/'native_ps5'/'ps5_launch.cpp', concepts/'ps5_launch.cpp')
    shutil.copy2(project/'native_ps5'/'ps5_launch.hpp', concepts/'ps5_launch.hpp')
    for file in ('state_store.cpp', 'state_store.hpp', 'cover_png.cpp', 'cover_png.hpp', 'cover_image.cpp', 'cover_image.hpp', 'emulator_covers.cpp', 'emulator_covers.hpp', 'root_manager.cpp', 'root_manager.hpp', 'ftp_readonly.cpp', 'ftp_readonly.hpp', 'ftp_scan.cpp', 'ftp_scan.hpp'):
        shutil.copy2(project/'native_ps5'/file, concepts/file)
    h = header.read_text(encoding='utf8')
    declaration = 'std::unique_ptr<app::Concept> make_universal_launcher(app::Context &context);'
    if declaration not in h:
        h = h.replace('} // namespace hui::concepts', declaration+'\n\n} // namespace hui::concepts')
        header.write_text(h,encoding='utf8')
    # The user requested ONE launcher, not the original 21 demonstration screens.
    registry.write_text("""// PS5 Universal Game Launcher. Includes exactly one application concept.
// SPDX-License-Identifier: MIT
#include "concepts/concepts.hpp"
namespace hui::app {
std::span<const ConceptFactory> concept_registry() {
    static constexpr ConceptFactory kFactories[] = {concepts::make_universal_launcher};
    return kFactories;
}
} // namespace hui::app
""", encoding='utf8')
    # The UI kit shell normally takes L1/R1 to switch between 21 demo screens.
    # With our single design, forward those exact actions to the launcher.
    shell=toolkit/'src'/'app'/'shell.cpp'
    if shell.is_file():
        original_shell=shell.read_text(encoding='utf8')
        if 'count > 1 && (input.is_pressed(Action::page_next)' not in original_shell:
            backup=shell.with_name('shell.cpp.ulg-backup')
            if not backup.exists():
                shutil.copy2(shell,backup)
            old='if (input.is_pressed(Action::page_next) || input.is_pressed(Action::page_prev))'
            if old not in original_shell:
                raise SystemExit('API der UI-Shell wurde geändert: L1/R1-Patch manuell prüfen.')
            original_shell=original_shell.replace(old,
                'if (count > 1 && (input.is_pressed(Action::page_next) || input.is_pressed(Action::page_prev)))',1)
            for suffix in ('pressed','released','held'):
                old='forwarded.'+suffix+' &= ~kShellActions;'
                if old not in original_shell:
                    raise SystemExit('UI-Shell-Eingabeänderung: kann sichere Weitergabe nicht belegen.')
                original_shell=original_shell.replace(old,
                    'forwarded.'+suffix+' &= ~(count == 1 ? 0u : kShellActions);',1)
            touch_old='if (input.is_pressed(Action::touch))'
            touch_new='if (count > 1 && input.is_pressed(Action::touch))'
            if touch_old not in original_shell:
                raise SystemExit('Touchpad-Patch: UI-Shell API wurde geaendert.')
            original_shell=original_shell.replace(touch_old,touch_new,1)
            shell.write_text(original_shell,encoding='utf8')
    destination=toolkit/'assets'/'paths.conf'
    shutil.copy2(project/'assets'/'paths.conf',destination)
    shutil.copy2(project/'assets'/'emulator_profiles.json', toolkit/'assets'/'emulator_profiles.json')
    shutil.copy2(project/'assets'/'cover_overrides.conf', toolkit/'assets'/'cover_overrides.conf')
    shutil.copy2(project/'assets'/'titles.conf', toolkit/'assets'/'titles.conf')
    shutil.copy2(project/'assets'/'bgh_logo.png', toolkit/'assets'/'bgh_logo.png')
    shutil.copy2(project/'assets'/'bgh_wordmark.png', toolkit/'assets'/'bgh_wordmark.png')
    shutil.copy2(project/'assets'/'premium_lake_background.png', toolkit/'assets'/'premium_lake_background.png')
    shutil.copy2(project/'assets'/'lake_background.png', toolkit/'assets'/'lake_background.png')
    shutil.copytree(project/'assets'/'ui', toolkit/'assets'/'ui', dirs_exist_ok=True)
    shutil.copytree(project/'assets'/'platforms', toolkit/'assets'/'platforms', dirs_exist_ok=True)
    # Keep the user-selected BGH tune as the sole background track, on the
    # kit's proven Vorbis player. No new synchronous decoder or audio thread.
    music_dir=toolkit/'assets'/'audio'/'music'
    music_dir.mkdir(parents=True,exist_ok=True)
    for old_music in music_dir.glob('*.ogg'):
        old_music.unlink()
    shutil.copy2(project/'assets'/'audio'/'music'/'BGH_EccoTidesOfTime.ogg',
                 music_dir/'BGH_EccoTidesOfTime.ogg')
    # Host-test mock kits have no main.cpp/audio backend. Real pinned kits do.
    if (toolkit/'src'/'main.cpp').is_file():
        # Make the one installed BGH track loop seamlessly. The kit's default
        # playlist pauses 1.5 seconds between songs, which is not the requested loop.
        music_source=toolkit/'src'/'audio'/'music.cpp'
        song_code=music_source.read_text(encoding='utf8')
        if 'track->set_looping(playlist_.size()==1);' not in song_code:
            old='track->set_looping(false);'
            if old not in song_code:
                raise SystemExit('BLOCKER: UI kit Vorbis playlist logic changed')
            music_source.write_text(song_code.replace(old,'track->set_looping(playlist_.size()==1);',1),encoding='utf8')
        # The upstream main owns the mixer. Read BGH's independent preference here,
        # not in the scene renderer (which has no mixer handle).
        entry=toolkit/'src'/'main.cpp'
        source=entry.read_text(encoding='utf8')
        marker='// BGH MUSIC SWITCH V1.0.12'
        if marker not in source:
            old='#include "audio/music.hpp"'
            if old not in source:raise SystemExit('BLOCKER: UI kit music header changed')
            source=source.replace(old,old+'\n#include "concepts/state_store.hpp"',1)
            before='    ps5::AudioOut audio_out;'
            replacement='''    // BGH MUSIC SWITCH V1.0.12: ULGM is isolated from favorites and recent.
        auto bgm_enabled_from_storage=[](){
            ulg::MusicSettings primary{}, fallback{};
            const bool have_primary=ulg::read_music_settings(
                "/download0/hui/ulg_v04/music.bin",primary);
            const bool have_fallback=ulg::read_music_settings(
                "/data/bgh/music.bin",fallback);
            if(have_fallback && (!have_primary || fallback.revision>primary.revision))
                return fallback.enabled;
            return have_primary?primary.enabled:true;
        };
        bool bgm_enabled=bgm_enabled_from_storage();
        std::int64_t next_bgm_check=0;
        mixer.set_bus_gain(audio::Bus::music,bgm_enabled?0.45f:0.0f);
        ps5::AudioOut audio_out;'''
            if before not in source:raise SystemExit('BLOCKER: UI kit audio output changed')
            source=source.replace(before,replacement,1)
            before='mixer.set_bus_gain(audio::Bus::music, Settings::gain(settings.music_volume));'
            replacement='mixer.set_bus_gain(audio::Bus::music, bgm_enabled ? Settings::gain(settings.music_volume) : 0.0f);'
            if before not in source:raise SystemExit('BLOCKER: music gain changed')
            source=source.replace(before,replacement,1)
            before='        music.pump(dt);'
            replacement='''        // Read tiny checksummed ULGM replicas at most every 250 ms; the game
            // thread never decodes music synchronously while drawing covers.
            if(now>=next_bgm_check){
                next_bgm_check=now+250000;
                const bool enabled=bgm_enabled_from_storage();
                if(enabled!=bgm_enabled){
                    bgm_enabled=enabled;
                    mixer.set_bus_gain(audio::Bus::music,
                        enabled?Settings::gain(shell.settings().music_volume):0.0f);
                }
            }
            if(bgm_enabled)music.pump(dt);'''
            if before not in source:raise SystemExit('BLOCKER: music pump changed')
            source=source.replace(before,replacement,1)
            entry.write_text(source,encoding='utf8')

    # cover_png.cpp uses zlib. The upstream boilerplate already builds pinned static zlib.
    # Modify the checkout Makefile only with a tagged, idempotent addition and backup.
    makefile=toolkit/'Makefile'
    original=makefile.read_text(encoding='utf8')
    marker='# PS5 UNIVERSAL LAUNCHER: PNG decode via pinned zlib'
    if marker not in original:
        backup=makefile.with_name('Makefile.ulg-backup')
        if not backup.exists():
            shutil.copy2(makefile,backup)
        inject=(marker+'\n'
                'APP_INCLUDE_PATHS += .deps/native/zlib/root/usr/include\n'
                'APP_STATIC_ARCHIVES += .deps/native/zlib/root/usr/lib/libz.a\n'
                '# Native PS5 registered-title launch, explicit controller action only\n'
                'APP_DEFINITIONS += BGH_TARGET_PS5=1\n'
                'APP_IMPORT_STUBS += .deps/native/ps5-payload-sdk/target/lib/libSceSystemService.so\n'
                'APP_IMPORT_STUBS += .deps/native/ps5-payload-sdk/target/lib/libSceUserService.so\n'
                '# Multi-controller APIs, linked through official SDK import stub\n'
                'APP_IMPORT_STUBS += .deps/native/ps5-payload-sdk/target/lib/libScePad.so\n')
        # Must appear AFTER the defaults and BEFORE the override APP_INCLUDE_PATHS.
        where='OPENGL_SDK := .deps/ps5-opengl/current'
        if where in original:
            original=original.replace(where,inject+where,1)
        else:
            # Minimal test templates may have no upstream SDK declarations.
            # Append only; real console builders must still validate linking.
            original=original.rstrip()+'\n\n'+inject
        makefile.write_text(original,encoding='utf8')
    print('Overlay eingebunden (GUI, Cache, PNG, Startadapter). NOCH KEINE PS5-APP GEBAUT.')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('toolkit_checkout',type=Path)
    args=parser.parse_args()
    apply(Path(__file__).resolve().parents[1],args.toolkit_checkout.resolve())
