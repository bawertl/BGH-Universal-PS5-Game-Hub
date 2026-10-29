"""V1.1.18 behavior and regression contracts; no PS5 runtime assertion."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/'native_ps5/universal_launcher.cpp').read_text()
WF=(ROOT/'.github/workflows/ps5-build.yml').read_text()

def section(a,b): return UI.split(a,1)[1].split(b,1)[0]

def test_platform_navigation_no_hidden_tabs():
    sidebar=section('void rebuild_sidebar()', 'bool persist_platform_registry(')
    assert 'items.push_back({"Alle Spiele"' in sidebar
    for hidden in ('Apps','Favoriten','Zuletzt'):
        assert f'items.push_back({{"{hidden}"' not in sidebar
    bar=section('void draw_platform_bar(', 'void draw_storage_widgets(')
    assert 'sidebar_items_.size()' in bar and 'const int index=first+slot;' in bar
    assert 'pill_width=142.0f' in bar and 'pill_step=147.0f' in bar
    assert 'switch_tab((tab_+1)%static_cast<int>(sidebar_items_.size()))' in UI
    assert 'recent_indices()' in UI and 'toggle_favorite()' in UI

def test_eight_tiles_are_real_navigation_not_visual_only():
    assert 'constexpr std::array<const char *,8> kSettingsTiles' in UI
    for title in ('Emulatoren & Spielordner','Sprache','Emulator-Startverhalten',
                  'Kachel-Design','Hintergrundmusik','System & Hilfe','Hintergrundbild','BGH-Logo'):
        assert f'"{title}"' in UI
    u=section('void update_settings(', 'void refresh_browser(')
    for token in ('settings_subview_==-1','selected_tile_','settings_subview_=selected_tile_',
                  'if(settings_subview_==0){update_emulator_settings',
                  'save_language_setting(', 'save_emulator_boot_setting(',
                  'save_theme_setting(', 'save_music_setting(',
                  'save_gallery();'):
        assert token in u
    assert 'if(settings_subview_>=0)settings_subview_=-1;' in UI
    assert 'void draw_settings_choice_detail(' in UI
    assert 'void draw_settings_gallery_detail(' in UI
    assert 'draw_settings_system(list)' in UI
    assert 'draw_settings_emulators(list)' in UI

def test_native_fonts_storage_and_recents():
    recent=section('void draw_recent_cards(', 'void draw_controllers(')
    storage=section('void draw_storage_widgets(', 'void draw_detail(')
    for view in (recent,storage):
        assert 'translated_text(list,context_.fonts.regular' in view
        assert 'serif_text(list,' not in view
    assert 'const Rect card{x,915,recent_card_width,110};' in recent
    assert 'recent_card_width=460.0f;' in recent
    assert 'shorten(g.title,51)' in recent
    assert 'const Rect panel{x,177,225,91};' in storage
    assert 'glass_card(list,panel,15);' in storage
    assert 'shadow_storage_' in storage

def test_unchanged_launch_and_storage_interfaces():
    for text in ('controllers_.observe_primary(input.connected);',
                 'controllers_.poll();',
                 'ulg::shadow_storage::fetch_storage()',
                 'stage_ps5sx2_disc_launch(g.path)',
                 'make_registered_ps5_launch_plan(g.title_id)',
                 'make_registered_ps4_launch_plan(g.title_id)',
                 'ulg::send_launch(plan)'):
        assert text in UI
    assert 'statvfs(' not in UI
    assert 'branches: [bgh-v1.1.31]' in WF
    assert 'BGH V1.1.31' in UI
