// PS5 Universal Game Launcher - UI component for blackbearreloaded/ps5-homebrew-ui.
// SPDX-License-Identifier: MIT
// Launch requests occur ONLY after explicit DualSense confirmation; no elevation or registration.
// To integrate, run tooling/install_ui_kit_overlay.py against a local checkout of ps5-homebrew-ui.
#include "concepts/concepts.hpp"
#include "concepts/library_core.hpp"
#include "concepts/root_manager.hpp"
#include "concepts/ftp_scan.hpp"
#include "concepts/state_store.hpp"
#include "concepts/cover_png.hpp"
#include "concepts/cover_image.hpp"
#include "concepts/emulator_covers.hpp"
#include "concepts/launcher_client.hpp"
#include "concepts/ps2_autostart.hpp"
#include "concepts/ps5_launch.hpp"
#include "concepts/playtime_log.hpp"
#include "concepts/ftp_readonly.hpp"
#include "controller_status.hpp"
#include "ui/glyphs.hpp"
#include "platform/ps5/system.hpp"
#include "ui/motion.hpp"
#include "premium_navigation.hpp"
#include "cover_load_debounce.hpp"
#include "ui_localization.hpp"
#include "serif_atlas_metrics.hpp"
#include "platform_registry.hpp"
#include "emulator_detection.hpp"
#include <algorithm>
#include <cmath>
#include <array>
#include <map>
#include <deque>
#include <cstdint>
#include <GL/glcorearb.h>
#include <cerrno>
#include <dirent.h>
#include <sys/types.h>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <ctime>
#include <utility>

// The PS5 payload SDK exports localtime_s(time_t*, tm*) but not localtime_r.
// Keep the platform declaration explicit: the SDK's C++ time header does
// not always advertise this libc symbol (see ps5-payload-dev/sdk/libc/localtime.c).
#if !defined(_WIN32) && !defined(__linux__) && !defined(__APPLE__)
extern "C" std::tm *localtime_s(const std::time_t *, std::tm *);
#endif

namespace hui::concepts {
namespace {
using gfx::Color;
using gfx::Rect;
const Color kInk = Color::rgb(0xF3E8D7);
const Color kMuted = Color::rgb(0xD5CCBE);
const Color kAccent = Color::rgb(0xC5D8B5);
const Color kSage = Color::rgb(0xCBD8B4);
const Color kWarmLine = Color::rgb(0xE9C17B);
constexpr std::array<const char *,8> kPlatforms={
    "PS5/PS4","PS5","PS4","PS2","Switch","Xbox 360","PS1","PSP"
};
constexpr const char *kPathKeys="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/_-.<+";
constexpr std::array<const char *,3> kSettingsTabs = {
    "Emulatoren & Spielordner", "Darstellung & Verhalten", "System & Hilfe"
};
// 1920x1080 logical canvas; 54 px safe TV margins. All layout is data-driven.
constexpr const char *kBuildLabel = "BGH V1.1.9-EMULATOR-AUTODETECTION [@BGH_GIT_COMMIT@]";
constexpr int kVisibleListRows = 9;
constexpr float kListTop = 192.0f;
constexpr float kListRowH = 79.0f;
constexpr float kGameListRight = 704.0f;
constexpr float kScreenRight = 1920.0f;
constexpr float kFanRightMargin = 54.0f;
constexpr float kFanCentreX = (kGameListRight + (kScreenRight-kFanRightMargin))*0.5f + 42.0f;
constexpr float kFanCentreY = 365.0f;
constexpr float kFanHeaderHeightRatio = 0.162f;
constexpr std::array<const char *, 9> kTabs = {
    "Alle Spiele", "PS5", "PS4", "PS2", "Switch", "Xbox 360", "Emulatoren", "Favoriten", "Zuletzt"
};
constexpr const char *kTechniques[] = {
    "Lokaler POSIX-Scan ohne Schreibzugriff auf Spieleordner",
    "Native OpenGL-GUI mit DualSense und Plattformfiltern",
    "PS5-Direktstart nach Benutzeraktion ueber sceSystemServiceLaunchApp (experimentell)"
};
std::string shorten(const std::string &s, std::size_t limit) {
    if(s.size() <= limit)return s;
    if(limit<4)return s.substr(0,limit);
    return s.substr(0,limit-3)+"...";
}
std::string display_platform_name(const std::string &platform){
    if(platform=="PS5")return "PlayStation 5";
    if(platform=="PS4")return "PlayStation 4";
    if(platform=="PS2")return "PlayStation 2";
    if(platform=="PS1")return "PlayStation 1";
    if(platform=="PSP")return "PlayStation Portable";
    if(platform=="PS3")return "PlayStation 3";
    if(platform=="Xbox")return "Original Xbox";
    if(platform=="Dreamcast")return "Sega Dreamcast";
    if(platform=="GameCube")return "Nintendo GameCube";
    if(platform=="Wii")return "Nintendo Wii";
    if(platform=="Switch")return "Nintendo Switch";
    if(platform=="Xbox 360")return "Xbox 360";
    if(platform=="Homebrew")return "Homebrew";
    // An ambiguous input such as "PlayStation" must not be guessed as PS2.
    return platform;
}
// These IDs already belong to the installed emulator entries in library_core.cpp.
// Do not dispatch arbitrary homebrew titles as emulator menu launches.
const char* emulator_platform_for_title(const std::string &id){
    if(id=="PPSA99203")return "PS2";
    if(id=="PPSA99008")return "Switch";
    if(id=="PPSA50011")return "Xbox 360";
    return nullptr;
}
ulg::LaunchPlan emulator_menu_plan(const std::string &platform){
    if(platform=="PS2")return ulg::make_ps5sx2_menu_launch_plan();
    if(platform=="Switch")return ulg::make_prosperoeden_menu_launch_plan();
    if(platform=="Xbox 360")return ulg::make_ps5x360_menu_launch_plan();
    return {};
}
std::string current_time_label(){
    const std::time_t now=std::time(nullptr);
    std::tm local_time{};
#if defined(_WIN32)
    if(localtime_s(&local_time,&now)!=0)return "--:--";
#elif defined(__linux__) || defined(__APPLE__)
    if(localtime_r(&now,&local_time)==nullptr)return "--:--";
#else
    // PS5's libc uses the C11/SDK argument order, unlike Windows.
    if(localtime_s(&now,&local_time)==nullptr)return "--:--";
#endif
    char buffer[6]{};
    return std::strftime(buffer,sizeof(buffer),"%H:%M",&local_time)>0?std::string(buffer):std::string("--:--");
}
Color platform_color(const std::string &p){
    if(p=="PS5")return Color::rgb(0xB7C8FF);
    if(p=="PS4")return Color::rgb(0xA9C8E6);
    if(p=="PS2")return Color::rgb(0xC9B8F7);
    if(p=="Switch")return Color::rgb(0xEAB4A5);
    if(p=="Xbox 360")return Color::rgb(0xB9D7B2);
    if(p=="Homebrew")return Color::rgb(0xDAC6ED);
    return Color::rgb(0xB3BBCB);
}
bool fan_cover_uses_case_header(const std::string &platform){
    return platform=="PS5" || platform=="PS4" || platform=="Switch";
}
const char* fan_cover_header_asset(const std::string &platform){
    if(platform=="PS5")return "/app0/assets/ui/fan_case_header_ps5.png";
    if(platform=="PS4")return "/app0/assets/ui/fan_case_header_ps4.png";
    if(platform=="Switch")return "/app0/assets/ui/fan_case_header_switch.png";
    return nullptr;
}
gfx::Rect uv_cover_fit(int image_width,int image_height,const gfx::Rect &target){
    if(image_width<=0 || image_height<=0 || target.w<=0.0f || target.h<=0.0f)return gfx::kFullUv;
    const float source_aspect=static_cast<float>(image_width)/static_cast<float>(image_height);
    const float target_aspect=target.w/target.h;
    if(std::abs(source_aspect-target_aspect)<0.0001f)return gfx::kFullUv;
    if(source_aspect>target_aspect){
        const float keep=target_aspect/source_aspect;
        const float offset=(1.0f-keep)*0.5f;
        return {offset,0.0f,keep,1.0f};
    }
    const float keep=source_aspect/target_aspect;
    const float offset=(1.0f-keep)*0.5f;
    return {0.0f,offset,1.0f,keep};
}
// Display the entire image without stretching when the original ratio is too
// extreme for the normal card silhouette. Normal box art fits its own card.
gfx::Rect cover_contain_rect(int image_width,int image_height,const gfx::Rect &card){
    if(image_width<=0 || image_height<=0 || card.w<=0.0f || card.h<=0.0f)return card;
    const float source_aspect=static_cast<float>(image_width)/static_cast<float>(image_height);
    const float target_aspect=card.w/card.h;
    if(source_aspect>target_aspect){
        const float height=card.w/source_aspect;
        return {card.x,card.y+(card.h-height)*0.5f,card.w,height};
    }
    const float width=card.h*source_aspect;
    return {card.x+(card.w-width)*0.5f,card.y,width,card.h};
}
// Derive the no-header card height from the decoded source (PS2/DVD and
// Xbox 360 formats vary between cover databases). This is *not* a global scale
// change: the five-cover fan spacing and width stay unchanged. The bounds
// prevent unusually wide banners or square thumbnails from making a giant or
// tiny card. In those cases cover_contain_rect() retains the entire artwork.
float fan_native_card_height(int image_width,int image_height,float width,float standard_height){
    if(image_width<=0 || image_height<=0 || width<=0.0f || standard_height<=0.0f)
        return standard_height;
    const float source_aspect=static_cast<float>(image_width)/static_cast<float>(image_height);
    if(!std::isfinite(source_aspect) || source_aspect<=0.0f)return standard_height;
    const float desired=width/source_aspect;
    return std::clamp(desired,standard_height*0.72f,standard_height*1.24f);
}
class CoverTextures {
public:
    // Called only from update() while a GL context is current; draw() performs no file I/O.
    bool load(const std::string &path) {
        if(path.empty() || textures_.count(path)) return false;
        if(failures_[path]>=2) return false;
        std::uint32_t texture = 0;
        ulg::PngImage image;
        std::string decode_error;
        const bool bundled_asset=path.rfind("/app0/assets/",0)==0;
        if((bundled_asset?ulg::decode_ui_image(path,image,&decode_error)
                         :ulg::decode_cover_image(path,image,&decode_error))) {
            GLuint gl_id=0;
            // Read any earlier GL error once before checking THIS upload.
            (void)glGetError();
            glGenTextures(1,&gl_id);
            if(gl_id){
                glBindTexture(GL_TEXTURE_2D,gl_id);
                glPixelStorei(GL_UNPACK_ALIGNMENT,4);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                const GLenum gl_error=glGetError();
                if(gl_error==GL_NO_ERROR)texture=gl_id;
                else {glDeleteTextures(1,&gl_id);errors_[path]="GPU/GL UPLOAD FEHLER "+std::to_string(gl_error);}
            }else errors_[path]="GPU/GL TEXTUR-ERZEUGUNG FEHLER";
        }else {
            decode_failures_.insert(path);
            errors_[path]="BILD-DEKODIERUNG: "+(decode_error.empty()?"UNBEKANNTER FEHLER":decode_error);
        }
        if(!texture){++failures_[path];return false;}
        failures_.erase(path);errors_.erase(path);decode_failures_.erase(path);
        // Covers are scaled to at most 512 px on their longest side; retain up to 256.
        if(order_.size()>=256){
            const auto previous=order_.front();order_.pop_front();
            const GLuint old=static_cast<GLuint>(textures_[previous]);
            if(old)glDeleteTextures(1,&old);
            textures_.erase(previous);
            sizes_.erase(previous);
        }
        textures_[path]=texture;sizes_[path]={image.width,image.height};order_.push_back(path);
        return true;
    }
    bool has(const std::string &path) const {
        const auto failure=failures_.find(path);
        return textures_.count(path)>0 || (failure!=failures_.end() && failure->second>=2);
    }
    bool decode_failed(const std::string &path) const {return decode_failures_.count(path)!=0;}
    std::string error(const std::string &path) const {
        const auto it=errors_.find(path);
        return it==errors_.end()?std::string{}:it->second;
    }
    std::uint32_t get(const std::string &path) const {
        const auto it=textures_.find(path);
        return it==textures_.end()?0:it->second;
    }
    std::pair<int,int> size(const std::string &path) const {
        const auto it=sizes_.find(path);
        return it==sizes_.end()?std::pair<int,int>{0,0}:it->second;
    }
    void clear(){
        for(const auto &item:textures_){const GLuint texture=static_cast<GLuint>(item.second);if(texture)glDeleteTextures(1,&texture);}
        textures_.clear();sizes_.clear();order_.clear();failures_.clear();errors_.clear();decode_failures_.clear();
    }
    ~CoverTextures(){clear();}
private:
    std::map<std::string,std::uint32_t> textures_;
    std::map<std::string,std::pair<int,int>> sizes_;
    std::map<std::string,unsigned> failures_;
    std::map<std::string,std::string> errors_;
    std::set<std::string> decode_failures_;
    std::deque<std::string> order_;
};
class UniversalLauncher final : public app::Concept {
public:
    explicit UniversalLauncher(app::Context &context) : context_(context) {
        // All writes are restricted to this title's own storage, never game directories.
        const auto prepare_directory=[&](const char *folder){
            if(mkdir(folder,0700)==0)return;
            const int error=errno;
            // EEXIST is only tentative: do not require opendir/lstat permissions.
            // A later fopen/write/rename in the PRIVATE cache verifies real access.
            if(error!=EEXIST && cache_error_.empty())
                cache_error_=std::string("BGH CACHE MKDIR: ")+folder+
                    " (errno "+std::to_string(error)+": "+std::strerror(error)+")";
        };
        // Try ALL known private paths independently. No mkdir error may prevent
        // lookup of already-readable artwork or the PS5 FTP cache.
        prepare_directory("/download0/hui");
        prepare_directory(kPrimaryStateFolder);
        prepare_directory(kFallbackStateFolder);
        prepare_directory("/download0/hui/ulg_v04/covers");
        prepare_directory("/download0/hui/ulg_v08");
        prepare_directory(kEmulatorCoverFolder);
        refresh_storage_directory_status();
        roots_=ulg::configured_roots(kUserPathsFile,"/app0/assets/paths.conf");
        // Upgrade old combined V0.8 roots to independently editable PS5/PS4
        // internal and external paths. Only BGH preferences are ever written.
        const bool migrated_native=ulg::ensure_native_roots(roots_);
        const bool migrated_ps4=ulg::ensure_ps4_install_root(roots_);
        if(migrated_native || migrated_ps4)
            (void)ulg::save_user_roots(roots_,kUserPathsFile);
        // Legacy private paths may have only /mnt/usb1/homebrew. Also scan
        // the USB parent for .exfat/.ffpfsc containers; never persist this
        // inferred root or override an explicitly disabled /mnt/usb1.
        (void)ulg::ensure_ps5_usb_parent_root(roots_);
        auto append_recent_tokens=[this](const std::vector<std::string> &tokens){
            for(const auto &key:tokens){
                if(recent_list_.size()>=80)break;
                if(std::find(recent_list_.begin(),recent_list_.end(),key)==recent_list_.end())
                    recent_list_.push_back(key);
            }
        };
        ulg::Preferences primary_preferences;
        const bool primary_read=ulg::read_preferences(kPrimaryPrefsFile,primary_preferences,&prefs_read_status_);
        if(primary_read){
            favorites_=primary_preferences.favorites;
            append_recent_tokens(primary_preferences.recent);
        }
        ulg::Preferences fallback_preferences;
        const bool fallback_read=ulg::read_preferences(kFallbackPrefsFile,fallback_preferences,&fallback_prefs_read_status_);
        if(fallback_read){
            favorites_.insert(fallback_preferences.favorites.begin(),fallback_preferences.favorites.end());
            append_recent_tokens(fallback_preferences.recent);
        }
        // Private recent backup files hold stable path identities and are merged
        // from both writable locations without overwriting either source.
        ulg::Preferences recent_backup;
        if(ulg::read_preferences(kPrimaryRecentBackupFile,recent_backup,&recent_read_status_))
            append_recent_tokens(recent_backup.recent);
        ulg::Preferences fallback_recent_backup;
        if(ulg::read_preferences(kFallbackRecentBackupFile,fallback_recent_backup,&fallback_recent_read_status_))
            append_recent_tokens(fallback_recent_backup.recent);
        // A missing file means the old proven direct-launch behavior.  Select
        // the newest valid replica; never touch/migrate ULGP preferences.bin.
        ulg::EmulatorBootSettings primary_boot{},fallback_boot{};
        const bool primary_boot_ok=ulg::read_emulator_boot_settings(
            kPrimaryEmulatorSettingsFile,primary_boot);
        const bool fallback_boot_ok=ulg::read_emulator_boot_settings(
            kFallbackEmulatorSettingsFile,fallback_boot);
        if(primary_boot_ok)emulator_boot_settings_=primary_boot;
        if(fallback_boot_ok && (!primary_boot_ok || fallback_boot.revision>primary_boot.revision))
            emulator_boot_settings_=fallback_boot;
        // Language lives in a separate ULGL file. Existing ULGP/ULGE are never
        // migrated or rewritten by language changes; newer valid replica wins.
        ulg::LanguageSettings primary_language{},fallback_language{};
        const bool primary_language_ok=ulg::read_language_settings(
            kPrimaryLanguageFile,primary_language);
        const bool fallback_language_ok=ulg::read_language_settings(
            kFallbackLanguageFile,fallback_language);
        if(primary_language_ok)language_settings_=primary_language;
        if(fallback_language_ok && (!primary_language_ok ||
           fallback_language.revision>primary_language.revision))
            language_settings_=fallback_language;
        // ULGT theme preference is independent from ULGP/ULGE/ULGL.
        ulg::ThemeSettings primary_theme{},fallback_theme{};
        const bool primary_theme_ok=ulg::read_theme_settings(kPrimaryThemeFile,primary_theme);
        const bool fallback_theme_ok=ulg::read_theme_settings(kFallbackThemeFile,fallback_theme);
        if(primary_theme_ok)theme_settings_=primary_theme;
        if(fallback_theme_ok && (!primary_theme_ok || fallback_theme.revision>primary_theme.revision))
            theme_settings_=fallback_theme;
        // ULGM sound toggle; never touches saved favorite or recent data.
        ulg::MusicSettings primary_music{},fallback_music{};
        const bool primary_music_ok=ulg::read_music_settings(kPrimaryMusicFile,primary_music);
        const bool fallback_music_ok=ulg::read_music_settings(kFallbackMusicFile,fallback_music);
        if(primary_music_ok)music_settings_=primary_music;
        if(fallback_music_ok && (!primary_music_ok ||
           fallback_music.revision>primary_music.revision))music_settings_=fallback_music;
        // Platform visibility/order is separate from ULGP favorites, history,
        // scan roots, music and emulator boot preferences.
        ulg::PlatformRegistry primary_platforms{},fallback_platforms{};
        const bool primary_platform_ok=ulg::read_platform_registry(kPrimaryPlatformFile,primary_platforms);
        const bool fallback_platform_ok=ulg::read_platform_registry(kFallbackPlatformFile,fallback_platforms);
        if(primary_platform_ok)platform_registry_=primary_platforms;
        if(fallback_platform_ok && (!primary_platform_ok ||
            fallback_platforms.revision>primary_platforms.revision))platform_registry_=fallback_platforms;
        // Bring existing scanner roots into the expanded multi-folder view.
        // This migration is in-memory until the user changes a profile; no
        // working scan paths, launches or history are rewritten on startup.
        for(auto &profile:platform_registry_.entries){
            if(!profile.rom_folder.empty() && profile.rom_folders.empty())
                (void)ulg::add_profile_folder(profile,profile.rom_folder);
            if(profile.scan_supported){
                for(const auto &root:roots_)if(root.platform==profile.id)
                    (void)ulg::add_profile_folder(profile,root.path);
            }
        }
        refresh_emulator_detections();
        rebuild_sidebar();
        for(const auto &id:recent_list_)recent_.insert(id);
        if(!ulg::read_catalogue_cache(kCacheFile,catalogue_))refresh();
        else{
            // Probe again without recursing. Cache must not hide removed USB drives.
            bool needs_ftp_reprobe=false;
            for(const auto &root:catalogue_.roots)if(root.status=="ftp_ok")needs_ftp_reprobe=true;
            bool roots_changed=catalogue_.roots.size()!=roots_.size();
            if(!roots_changed)for(std::size_t i=0;i<roots_.size();++i){
                if(catalogue_.roots[i].platform!=roots_[i].platform ||
                   catalogue_.roots[i].path!=roots_[i].path ||
                   (catalogue_.roots[i].status=="deaktiviert")!=(!roots_[i].enabled)){
                    roots_changed=true;break;
                }
            }
            bool missing_usb_ps5_cover=false;
            for(const auto &g:catalogue_.games){
                if(g.platform=="PS5" && g.format=="installationsordner" &&
                   g.cover_path.empty() && g.path.compare(0,8,"/mnt/usb")==0){
                    missing_usb_ps5_cover=true;break;
                }
            }
            // USB media may have been detached and altered on a PC even if
            // the mount path and root count are unchanged. Re-enumerate at
            // application start, never during per-frame cover rendering.
            bool has_external_root=false;
            for(const auto &r:roots_)if(r.enabled &&
               (r.path.compare(0,8,"/mnt/usb")==0 || r.path.compare(0,8,"/mnt/ext")==0)){
                has_external_root=true;break;
            }
            // An empty, cached cover path must not become a permanent answer.
            if(has_external_root || roots_changed || catalogue_.failed_operations>0 ||
               needs_ftp_reprobe || missing_usb_ps5_cover)refresh();
            else {validate_cached_roots();
            apply_presentation();
            ulg::clean_catalogue(catalogue_,roots_);
            rebuild();}
        }
        // One bounded, read-only refresh per BGH start. No per-frame FTP reads.
        // The hero line can show the most recent completed native-game session.
        refresh_playtime_log();
    }
    const app::ConceptInfo &info() const override {
        static const app::ConceptInfo kInfo{
            "universal_launcher", "BGH - BawertL's Game Hub",
            "Alle Plattformen. Eine Bibliothek.",
            "src/concepts/universal_launcher.cpp", audio::SoundSet::glass, kAccent, kTechniques
        };
        return kInfo;
    }
    void enter() override { clock_=0.0f; visual_selection_=static_cast<float>(selected_);
        list_scroll_=ulg::premium::scroll_target(selected_,indices_.size(),kVisibleListRows); }
    void update(const InputFrame &input, float dt, app::Feedback &feedback) override {
        clock_ += dt;
        // Debounce new, synchronous cover decodes during fast navigation.
        // Bundled assets and the three recent cards are always serviced first.
        const bool browsing_screen=!settings_ && !detail_ && !search_open_ &&
            !browser_open_ && !path_editor_open_ && !recent_focus_;
        const bool navigation_event=browsing_screen &&
            (input.nav!=Direction::none || input.is_pressed(Action::page_prev) ||
             input.is_pressed(Action::page_next));
        cover_load_debounce_.advance(dt,navigation_event,selected_,tab_);
        if(clock_>=next_controller_poll_){controllers_.poll();next_controller_poll_=clock_+1.0f;}
        // Real connected status from the UI kit's primary DualSense, not a guess.
        controllers_.observe_primary(input.connected);
        // A single authoritative game index drives the list, fan and information.
        // Animation always follows the CURRENT target; quick key presses never queue tweens.
        const float target=static_cast<float>(selected_);
        visual_selection_=ulg::premium::approach(visual_selection_,target,dt,13.5f);
        list_scroll_=ulg::premium::approach(list_scroll_,
            ulg::premium::scroll_target(selected_,indices_.size(),kVisibleListRows),dt,15.0f);
        if(!covers_.has("/app0/assets/bgh_logo.png"))covers_.load("/app0/assets/bgh_logo.png");
        if(!covers_.has("/app0/assets/premium_lake_background.png"))covers_.load("/app0/assets/premium_lake_background.png");
        if(!covers_.has("/app0/assets/ui/frosted_lake_backdrop.png"))covers_.load("/app0/assets/ui/frosted_lake_backdrop.png");
        if(!covers_.has("/app0/assets/ui/serif_atlas.png"))covers_.load("/app0/assets/ui/serif_atlas.png");
        // One tiny bundled icon per frame; never synchronously load a whole
        // future-platform icon pack during scrolling.
        for(const auto &p:platform_registry_.entries){
            if(p.icon_path.empty())continue;
            const std::string path=std::string("/app0/assets/platforms/")+p.icon_path;
            if(!covers_.has(path)){covers_.load(path);break;}
        }
        load_next_visible_cover();
        if(input.is_pressed(Action::back)){
            if(path_editor_open_){path_editor_open_=false;emulator_path_edit_=false;feedback.play(audio::Cue::back);}
            else if(browser_open_){browser_open_=false;emulator_path_edit_=false;feedback.play(audio::Cue::back);}
            else if(search_open_){search_open_=false;feedback.play(audio::Cue::back);}
            else if(detail_ || settings_){detail_=false;settings_=false;feedback.play(audio::Cue::back);}
            else if(recent_focus_){recent_focus_=false;feedback.play(audio::Cue::back);}
            return;
        }
        if(input.is_pressed(Action::menu)){
            detail_=false;search_open_=false;browser_open_=false;path_editor_open_=false;
            settings_=!settings_;
            if(settings_){settings_tab_=0;probe_recent_storage();}
            feedback.play(audio::Cue::open);return;
        }
        if(search_open_){update_search(input,feedback);return;}
        if(path_editor_open_){update_path_editor(input,feedback);return;}
        if(browser_open_){update_browser(input,feedback);return;}
        if(settings_){update_settings(input,feedback);return;}
        if(detail_){
            if(input.is_pressed(Action::north))toggle_favorite();
            if(input.is_pressed(Action::west) && !indices_.empty()){
                const auto &g=catalogue_.games[indices_[static_cast<std::size_t>(selected_)]];
                if(g.platform=="PS5" || g.platform=="PS4"){
                    // Diagnostic only, explicitly triggered by Square in native-game detail.
                    // Neither launcher nor elfldr receives any application data.
                    local_services_=ulg::probe_local_launch_services();
                    local_services_checked_=true;
                    feedback.play(audio::Cue::select);
                    return;
                }
                if(g.platform=="PS2"){
                    // Manual PS5SX2 menu fallback: Square sends NO game request.
                    const auto result=ulg::send_launch(ulg::make_ps5sx2_menu_launch_plan());
                    launch_message_=result.message;
                    if(result.status==ulg::LaunchStatus::started || result.status==ulg::LaunchStatus::queued){
                        feedback.play(audio::Cue::launch);
                        if(result.status==ulg::LaunchStatus::queued)sys::quit();
                    }else feedback.play(audio::Cue::error);
                    return;
                }
                if(g.platform!="Switch" && g.platform!="Xbox 360")return;
                // The direct system-service path was refused with 0x8094000F on
                // actual hardware. The existing Forwarder handles the app switch.
                const auto result=ulg::send_launch(emulator_menu_plan(g.platform));
                launch_message_=result.message;
                if(result.status==ulg::LaunchStatus::queued ||
                   result.status==ulg::LaunchStatus::started){
                    feedback.play(audio::Cue::launch);
                    if(result.status==ulg::LaunchStatus::queued)sys::quit();
                }else feedback.play(audio::Cue::error);
                // Opening a menu is not a played game and must not fill recents.
                return;
            }
            if(input.is_pressed(Action::confirm))request_selected_launch(feedback);
            return;
        }
        // Touchpad gives focus to the three recent request cards.
        if(input.is_pressed(Action::touch)){
            recent_focus_=!recent_focus_;recent_selected_=0;
            launch_in_progress_=false;
            feedback.play(audio::Cue::focus);return;
        }
        if(recent_focus_){
            const auto recent=recent_indices();
            if(input.nav==Direction::up){recent_focus_=false;launch_in_progress_=false;return;}
            if(input.nav==Direction::left)recent_selected_=std::max(0,recent_selected_-1);
            if(input.nav==Direction::right)recent_selected_=std::min(2,recent_selected_+1);
            // L3 inspects the selected recent title; X launches it directly.
            if(input.is_pressed(Action::l3) || input.is_pressed(Action::confirm)){
                const bool details=input.is_pressed(Action::l3);
                if(recent[recent_selected_]>=0 && focus_recent_game(recent[recent_selected_])){
                    if(details){refresh_playtime_log();detail_=true;feedback.play(audio::Cue::open);}
                    else request_selected_launch(feedback);
                }
            }
            return;
        }
        // L3 opens details. L1/R1 return to pure platform cycling.
        if(input.is_pressed(Action::l3) && !sidebar_focus_ && !indices_.empty()){
            refresh_playtime_log();detail_=true;launch_message_.clear();local_services_checked_=false;
            feedback.play(audio::Cue::open);return;
        }
        if(input.nav==Direction::left && !sidebar_focus_){
            sidebar_focus_=true;launch_in_progress_=false;
            feedback.play(audio::Cue::focus);return;
        }
        if(sidebar_focus_){
            if(input.nav==Direction::right){sidebar_focus_=false;feedback.play(audio::Cue::focus);return;}
            if(input.nav==Direction::up){switch_tab((tab_+static_cast<int>(sidebar_items_.size())-1)%static_cast<int>(sidebar_items_.size()));feedback.play(audio::Cue::tab);return;}
            if(input.nav==Direction::down){switch_tab((tab_+1)%static_cast<int>(sidebar_items_.size()));feedback.play(audio::Cue::tab);return;}
            if(input.is_pressed(Action::confirm)){sidebar_focus_=false;feedback.play(audio::Cue::select);return;}
        }
        if(input.is_pressed(Action::page_prev)){
            switch_tab((tab_+static_cast<int>(sidebar_items_.size())-1)%static_cast<int>(sidebar_items_.size()));
            feedback.play(audio::Cue::tab);return;
        }
        if(input.is_pressed(Action::page_next)){
            switch_tab((tab_+1)%static_cast<int>(sidebar_items_.size()));
            feedback.play(audio::Cue::tab);return;
        }
        if(input.is_pressed(Action::west)){refresh();feedback.play(audio::Cue::select);return;}
        if(input.is_pressed(Action::north)){
            search_open_=true;search_key_=0;feedback.play(audio::Cue::open);return;
        }
        if(input.nav!=Direction::none && !indices_.empty()){
            const int delta=(input.nav==Direction::left || input.nav==Direction::up)?-1:1;
            const int next=ulg::premium::step(selected_,delta,indices_.size());
            if(next!=selected_){selected_=next;launch_in_progress_=false;feedback.play(audio::Cue::focus);}
        }
        // One X starts the currently selected actual game without any detail overlay.
        if(input.is_pressed(Action::confirm) && !sidebar_focus_)request_selected_launch(feedback);
    }
    void draw(app::Frame &frame) const override {
        frame.backdrop.mode=gfx::BackdropMode::aurora;
        frame.backdrop.colors[0]=Color::rgb(0x101722);
        frame.backdrop.colors[1]=Color::rgb(0x1D2430);
        frame.backdrop.colors[2]=Color::rgb(0x1A2431);
        frame.backdrop.colors[3]=Color::rgb(0x101722);
        frame.backdrop.time=0.0f;
        auto &list=frame.scene;
        const auto &fonts=context_.fonts;
        const auto background=covers_.get("/app0/assets/premium_lake_background.png");
        if(background)list.image(background,{0,0,1920,1080},gfx::kFullUv,Color::rgb(0xFFFFFF),0);
        else list.rounded_rect({0,0,1920,1080},0,Color::rgb(0x152233));
        list.rounded_rect({0,0,1920,1080},0,Color::rgb(0x05080C,0.10f));
        glass_card(list,{12,113,247,823},28);
        glass_card(list,{272,113,432,823},28);
        // The fan floats above the scenery; no static panel behind moving covers.
        // The title, platform badge and start hint below are free-standing too.
        const auto brand=covers_.get("/app0/assets/bgh_logo.png");
        if(brand)list.image(brand,{30,16,73,73},gfx::kFullUv,Color::rgb(0xFFFFFF),12);
        const auto wordmark=covers_.get("/app0/assets/bgh_wordmark.png");
        if(wordmark)list.image(wordmark,{103,24,432,71},gfx::kFullUv,Color::rgb(0xFFFFFF),0);
        else serif_text(list,"BawertL's Game Hub",112,60,31,kInk,423);
        // Open header: no large outer capsule, just the count chip and matching divider.
        // Keep their visual centres aligned, with breathing room above the first row.
        serif_text(list,"Meine Spiele",312,158,30,kInk,270);
        const Rect count_chip{612,131,60,31};
        list.rounded_rect(count_chip,15,Color::rgb(0xF2E3C2,0.16f));
        list.bordered_rect(count_chip,15,Color::rgb(0xFFFFFF,0),1.0f,Color::rgb(0xF6E5C4,0.35f));
        const std::string count_text=std::to_string(indices_.size());
        const float count_size=18.0f;
        const float count_x=count_chip.x+(count_chip.w-serif_text_width(count_text,count_size))*0.5f;
        serif_text(list,count_text,count_x,153,count_size,Color::rgb(0xF2D7A1));
        list.rounded_rect({300,176,386,2},1,Color::rgb(0xE6D6B4,0.32f));
        draw_controllers(list);
        const std::string time_text=current_time_label();
        serif_text(list,time_text,1660,58,24,kInk,90);
        const auto search_icon=covers_.get("/app0/assets/ui/search.png");
        if(search_icon)list.image(search_icon,{1766,29,34,34},gfx::kFullUv,Color::rgb(0xF8EEDB),0);
        const auto settings_icon=covers_.get("/app0/assets/ui/settings.png");
        if(settings_icon)list.image(settings_icon,{1822,29,34,34},gfx::kFullUv,Color::rgb(0xF8EEDB),0);
        draw_platform_bar(list);
        draw_game_list(list);
        if(indices_.empty()){
            serif_text(list,"Keine Spiele in dieser Kategorie",792,460,37,kInk,700);
        }else{
            draw_cover_fan(list);
            draw_game_info(list);
        }
        draw_recent_cards(list);
        const ui::Hint hints[]={
            {ui::Button::cross,hint("Start/Menue")},{ui::Button::circle,hint("Zurueck")},
            {ui::Button::l1,"Plattform -"},{ui::Button::r1,"Plattform +"},
            {ui::Button::left_stick,hint("Details")},{ui::Button::square,hint("Aktualisieren")},
            {ui::Button::triangle,hint("Suche")},{ui::Button::options,hint("Einstellungen")}
        };
        ui::HintLayout hint_layout;hint_layout.cy=1044;hint_layout.size=24;
        hint_layout.text_size=17;hint_layout.item_gap=24;
        ui::draw_hints(list,fonts,ui::GlyphStyle::dark(),hints,8,43,false,hint_layout);
        if(detail_)draw_detail(frame);
        if(settings_)draw_settings(frame);
        if(browser_open_)draw_browser(frame);
        if(path_editor_open_)draw_path_editor(frame);
        if(search_open_)draw_search(frame);
        // Lower-right, inside the TV safe area. Kept in sync with the build branch.
        const std::string release_label="BGH V1.1.9";
        const std::string release_text=release_label+" Container-Launch-Title-Fix";
        const float release_width=serif_text_width(release_text,17.0f);
        translated_text(list,fonts.regular,release_text,1860.0f-release_width,1045,17,
            Color::rgb(0xFFFFFF,0.58f));
    }

private:
    static constexpr const char *kPrimaryStateFolder="/download0/hui/ulg_v04";
    static constexpr const char *kFallbackStateFolder="/data/bgh";
    static constexpr const char *kEmulatorCoverFolder="/download0/hui/ulg_v08/covers";
    // New cache forces one metadata/cover rescan; preserve the V0.6 cache and user preferences.
    // New private cache forces a one-time fresh catalogue rebuild from actual settings.
    // Never delete old cache files; prior working releases may continue using them.
    // Keep the previous library_v129.bin untouched for rollbacks. A new private
    // snapshot forces one fresh FTP cover probe after installing this fix.
    // A new BGH-owned snapshot forces a complete first scan after this repair.
    // Existing V1.1.3 cache library_v113_ps5_cover.bin remains untouched for rollback.
    static constexpr const char *kCacheFile="/download0/hui/ulg_v04/library_v116_container_start_title.bin";
    static constexpr const char *kUserPathsFile="/download0/hui/ulg_v04/paths.user.conf";
    static constexpr const char *kPrimaryEmulatorSettingsFile="/download0/hui/ulg_v04/emulator_boot.bin";
    static constexpr const char *kFallbackEmulatorSettingsFile="/data/bgh/emulator_boot.bin";
    static constexpr const char *kPrimaryLanguageFile="/download0/hui/ulg_v04/language.bin";
    static constexpr const char *kFallbackLanguageFile="/data/bgh/language.bin";
    static constexpr const char *kPrimaryThemeFile="/download0/hui/ulg_v04/theme.bin";
    static constexpr const char *kPrimaryMusicFile="/download0/hui/ulg_v04/music.bin";
    static constexpr const char *kPrimaryPlatformFile="/download0/hui/ulg_v04/platforms.bin";
    static constexpr const char *kFallbackPlatformFile="/data/bgh/platforms.bin";
    static constexpr const char *kFallbackMusicFile="/data/bgh/music.bin";
    static constexpr const char *kFallbackThemeFile="/data/bgh/theme.bin";
    static constexpr const char *kPrimaryPrefsFile="/download0/hui/ulg_v04/preferences.bin";
    static constexpr const char *kPrimaryRecentBackupFile="/download0/hui/ulg_v04/recent_v123.bin";
    static constexpr const char *kFallbackPrefsFile="/data/bgh/preferences.bin";
    static constexpr const char *kFallbackRecentBackupFile="/data/bgh/recent_v123.bin";
    void apply_presentation(){
        // No game paths or installations are modified. Overrides use library metadata only.
        ulg::apply_title_overrides(catalogue_,"/app0/assets/titles.conf");
        ulg::apply_title_overrides(catalogue_,"/download0/hui/ulg_v04/titles.user.conf");
        // Presentation-only: keep stable game IDs, paths and start arguments unchanged.
        // Also cleans existing cached/metadata titles, not only fresh filenames.
        for(auto &game:catalogue_.games){
            if(game.platform=="PS5" && (game.format=="ffpfsc" || game.format=="exfat"))
                game.title=ulg::ps5_container_fallback_title(game.title,game.title_id);
            game.title=ulg::clean_title_for_display(game.title);
        }
        ulg::apply_cover_overrides(catalogue_,"/app0/assets/cover_overrides.conf");
        ulg::apply_cover_overrides(catalogue_,"/download0/hui/ulg_v04/covers.user.conf");
    }
    bool focus_recent_game(int catalogue_index){
        if(catalogue_index<0 || static_cast<std::size_t>(catalogue_index)>=catalogue_.games.size())return false;
        const auto &game=catalogue_.games[static_cast<std::size_t>(catalogue_index)];
        const std::string stable_key="PATH|"+game.platform+"|"+game.path;
        search_query_.clear();switch_tab(0);
        for(std::size_t i=0;i<indices_.size();++i){
            const auto &candidate=catalogue_.games[indices_[i]];
            if(candidate.id==game.id || recent_matches(candidate,stable_key)){
                selected_=static_cast<int>(i);
                visual_selection_=static_cast<float>(selected_);
                list_scroll_=ulg::premium::scroll_target(selected_,indices_.size(),kVisibleListRows);
                recent_focus_=false;launch_in_progress_=false;
                launch_message_.clear();local_services_checked_=false;
                return true;
            }
        }
        launch_message_="Spiel aus dem Verlauf ist aktuell nicht in der Bibliothek";
        return false;
    }
    // The exact V0.12.23 per-platform request paths are retained below.
    // Only accepted/queued requests enter recent history; this is NOT runtime proof.
    void request_selected_launch(app::Feedback &feedback){
        if(indices_.empty() || launch_in_progress_ || clock_<launch_retry_after_)return;
        const auto &selected_game=catalogue_.games[indices_[static_cast<std::size_t>(selected_)]];
        // Emulator entries are registered homebrew apps, not individual games.
        // Bypass ROM dispatch and recent history in EVERY view (also Favorites).
        // The IDs are the known, source-verified installed emulator IDs.
        if(selected_game.platform=="Homebrew"){
            launch_retry_after_=clock_+1.0f;
            launch_request_accepted_=false;
            const char *emulator_platform=emulator_platform_for_title(selected_game.title_id);
            if(!emulator_platform){
                launch_message_="Keine verifizierte Emulator-Startschnittstelle fuer diesen Eintrag";
                feedback.play(audio::Cue::error);
                return;
            }
            // All three known emulator app menus use the same Forwarder as
            // PS5SX2. Do not pass any game path or add a recent-history entry.
            const auto result=ulg::send_launch(emulator_menu_plan(emulator_platform));
            launch_message_=result.message;
            if(result.status==ulg::LaunchStatus::queued ||
               result.status==ulg::LaunchStatus::started){
                feedback.play(audio::Cue::launch);
                if(result.status==ulg::LaunchStatus::queued)sys::quit();
            }else feedback.play(audio::Cue::error);
            return; // No recent-history mutation or ROM/path argument.
        }
        // Menu-only mode never stages a disc-launch.txt, passes ROM paths,
        // or marks a game as played. Native PS5/PS4 requests are unaffected.
        if(!emulator_boot_settings_.direct_boot &&
           (selected_game.platform=="PS2" || selected_game.platform=="Switch" ||
            selected_game.platform=="Xbox 360")){
            launch_retry_after_=clock_+1.0f;
            launch_request_accepted_=false;
            // Menu-only start must not require the chosen ROM/USB to be present:
            // this request carries a registered app ID and zero arguments.
            const auto result=ulg::send_launch(emulator_menu_plan(selected_game.platform));
            launch_message_=result.message;
            if(result.status==ulg::LaunchStatus::queued ||
               result.status==ulg::LaunchStatus::started){
                feedback.play(audio::Cue::launch);
                if(result.status==ulg::LaunchStatus::queued)sys::quit();
            }else feedback.play(audio::Cue::error);
            return;
        }
        launch_in_progress_=true;
        launch_retry_after_=clock_+1.0f;
        launch_message_.clear();
        launch_request_accepted_=false;
                const auto &g=catalogue_.games[indices_[static_cast<std::size_t>(selected_)]];
                const auto plan=ulg::make_launch_plan(g.platform,g.path);
                if(!g.available){launch_message_="USB-Laufwerk / Spiel nicht verfügbar";feedback.play(audio::Cue::error);}
                else if(g.platform=="PS5" && (g.format=="ffpfsc" || g.format=="exfat")){
                    // Controlled experiment: system-menu launch was confirmed
                    // for registered PPSA06084 on FW 13.60. This does NOT
                    // imply other images are mounted or registered. Dispatch
                    // the TITLE ID only, never the image path or mount command.
                    const auto ps5_plan=ulg::make_registered_ps5_launch_plan(g.title_id);
                    if(!ps5_plan.supported){
                        launch_message_="Container: ungueltige/ausgeschlossene PPSA-ID";
                        feedback.play(audio::Cue::error);
                    }else{
                        const auto previous_recent=recent_list_;
                        record_recent(g.id); // Tentative; rollback on refusal.
                        const auto bridge=ulg::send_launch(ps5_plan);
                        if(bridge.status==ulg::LaunchStatus::queued ||
                           bridge.status==ulg::LaunchStatus::started){
                            launch_message_="Container-Startauftrag angenommen; Spielstart nicht verifiziert";
                            launch_request_accepted_=true;
                            feedback.play(audio::Cue::launch);
                            if(bridge.status==ulg::LaunchStatus::queued)sys::quit();
                        }else{
                            rollback_recent(previous_recent);
                            launch_message_=bridge.status==ulg::LaunchStatus::no_launcher
                                ?"Launcher-ELF auf Port 10199 nicht erreichbar"
                                :"Container-Start abgelehnt: "+bridge.message;
                            feedback.play(audio::Cue::error);
                        }
                    }
                }
                else if(g.platform=="PS5"){
                    // Prefer an ALREADY RUNNING, user-provided PS5 forwarder
                    // launcher. Never install/inject payloads or modify system DB.
                    // Both backends accept registered TITLE IDs, not scan paths.
                    const auto ps5_plan=ulg::make_registered_ps5_launch_plan(g.title_id);
                    if(!ps5_plan.supported){
                        launch_message_=ps5_plan.reason;
                        feedback.play(audio::Cue::error);
                    }else{
                        const auto previous_recent=recent_list_;
                        record_recent(g.id);  // Persist before the forwarder can switch apps.
                        const auto bridge=ulg::send_launch(ps5_plan);
                        if(bridge.status==ulg::LaunchStatus::queued ||
                           bridge.status==ulg::LaunchStatus::started){
                            launch_message_=bridge.status==ulg::LaunchStatus::queued
                                ? "Startdienst hat PS5-Start eingereiht; BGH wird verlassen"
                                : "Startdienst hat Start bestaetigt; Spielstart noch nicht verifiziert";
                            launch_request_accepted_=true;
                            feedback.play(audio::Cue::launch);
                            // With ALREADY_RUNNING_KILL_NEEDED, the daemon
                            // needs the foreground app to exit to complete launch.
                            if(bridge.status==ulg::LaunchStatus::queued)sys::quit();
                        }else if(bridge.status==ulg::LaunchStatus::no_launcher){
                            // Preserve the old method as a fallback; no service
                            // or payload is started behind the user's back.
                            const auto native=ulg::launch_registered_ps5(g.title_id);
                            launch_message_="Kein Dienst auf 127.0.0.1:10199; "+native.message;
                            if(native.accepted){
                                launch_request_accepted_=true;feedback.play(audio::Cue::launch);
                            }else{
                                rollback_recent(previous_recent);feedback.play(audio::Cue::error);
                            }
                        }else{
                            // A daemon refusal is NOT an accepted launch.
                            char rc_text[82];
                            std::snprintf(rc_text,sizeof(rc_text),
                                " [rc1=0x%08X rc2=0x%08X]",
                                bridge.rc1,bridge.rc2);
                            launch_message_=bridge.message+rc_text;
                            rollback_recent(previous_recent);
                            feedback.play(audio::Cue::error);
                        }
                    }
                }
                else if(g.platform=="PS4"){
                    // First CUSA hardware experiment. Uses the existing, proven
                    // PFWL send_launch() and an ALREADY RUNNING launcher only.
                    // Unlike PS5, deliberately NO native fallback on no_launcher.
                    const auto ps4_plan=ulg::make_registered_ps4_launch_plan(g.title_id);
                    if(!ps4_plan.supported){
                        launch_message_=ps4_plan.reason;
                        feedback.play(audio::Cue::error);
                    }else{
                        const auto previous_recent=recent_list_;
                        record_recent(g.id);
                        const auto bridge=ulg::send_launch(ps4_plan);
                        if(bridge.status==ulg::LaunchStatus::queued ||
                           bridge.status==ulg::LaunchStatus::started){
                            launch_message_=bridge.status==ulg::LaunchStatus::queued
                                ? "Startdienst hat PS4-Start eingereiht; BGH wird verlassen"
                                : "PS4-Start vom Dienst bestaetigt; Wechsel noch nicht verifiziert";
                            launch_request_accepted_=true;
                            feedback.play(audio::Cue::launch);
                            // Only the queued status requires BGH to exit.
                            // Keep started observable during this first PS4 test.
                            if(bridge.status==ulg::LaunchStatus::queued)sys::quit();
                        }else if(bridge.status==ulg::LaunchStatus::no_launcher){
                            launch_message_="Kein Startdienst auf 127.0.0.1:10199 (PS4)";
                            rollback_recent(previous_recent);
                            feedback.play(audio::Cue::error);
                        }else{
                            char rc_text[82];
                            std::snprintf(rc_text,sizeof(rc_text),
                                " [rc1=0x%08X rc2=0x%08X]",
                                bridge.rc1,bridge.rc2);
                            launch_message_=bridge.message+rc_text;
                            rollback_recent(previous_recent);
                            feedback.play(audio::Cue::error);
                        }
                    }
                }
                else if(g.platform=="PS2"){
                    // PS5SX2 release (vk-285-171) reads and consumes
                    // /data/PCSX2/logs/disc-launch.txt on boot, skipping its shelf.
                    // Only X uploads the chosen game path through localhost FTP.
                    // No direct POSIX writes: app sandbox and FTP views may differ.
                    // Existing remote requests must never be overwritten.
                    const auto request=ulg::stage_ps5sx2_disc_launch(g.path);
                    if(!request.ready){
                        launch_message_="PS2-AUTOSTART: "+request.message+
                            (request.os_error ? " (errno "+std::to_string(request.os_error)+")" :
                            " (FTP "+std::to_string(request.ftp_reply)+")");
                        feedback.play(audio::Cue::error);
                    }else{
                        const auto ps2_plan=ulg::make_ps5sx2_menu_launch_plan();
                        const auto previous_recent=recent_list_;
                        record_recent(g.id);
                        const auto bridge=ulg::send_launch(ps2_plan);
                        if(bridge.status==ulg::LaunchStatus::queued ||
                           bridge.status==ulg::LaunchStatus::started){
                            launch_message_=bridge.status==ulg::LaunchStatus::queued
                                ? "PS2-Spielstart eingereiht; BGH wird verlassen"
                                : "PS2-Spielstart angefragt; PS5SX2 wird geoeffnet";
                            launch_request_accepted_=true;
                            feedback.play(audio::Cue::launch);
                            if(bridge.status==ulg::LaunchStatus::queued)sys::quit();
                        }else{
                            // Do not leave a stale request that would hijack a
                            // later ordinary PS5SX2 menu launch.
                            const bool cleaned=ulg::cancel_ps5sx2_disc_launch(request);
                            if(bridge.status==ulg::LaunchStatus::no_launcher){
                                launch_message_="Kein Startdienst auf 127.0.0.1:10199 (PS5SX2)";
                            }else{
                                char rc_text[82];
                                std::snprintf(rc_text,sizeof(rc_text),
                                    " [rc1=0x%08X rc2=0x%08X]",bridge.rc1,bridge.rc2);
                                launch_message_="PS5SX2: "+bridge.message+rc_text;
                            }
                            if(!cleaned)launch_message_+="; FTP-Startdatei bitte pruefen";
                            rollback_recent(previous_recent);
                            feedback.play(audio::Cue::error);
                        }
                    }
                }
                else if(!plan.supported){launch_message_=plan.reason;feedback.play(audio::Cue::error);}
                else {
                    struct stat current{};
                    if((!g.via_ftp && lstat(g.path.c_str(),&current)!=0) ||
                       (!g.via_ftp && !S_ISREG(current.st_mode) && !S_ISDIR(current.st_mode))){
                        launch_message_="Spiel / USB-Datentraeger nicht mehr verfuegbar";
                        feedback.play(audio::Cue::error);
                    }else{
                        const auto previous_recent=recent_list_;
                        record_recent(g.id);
                        const auto result=ulg::send_launch(plan);
                        launch_message_=result.message;
                        if(result.status==ulg::LaunchStatus::queued || result.status==ulg::LaunchStatus::started){
                            launch_request_accepted_=true;feedback.play(audio::Cue::launch);
                            if(result.status==ulg::LaunchStatus::queued)sys::quit();
                        }else{
                            rollback_recent(previous_recent);feedback.play(audio::Cue::error);
                        }
                    }
                }
        // Keep an accepted request guarded until the user changes focus or returns.
        // Failed requests release the guard, with a short retry debounce.
        if(!launch_request_accepted_)launch_in_progress_=false;
    }

    // V0.12.27 diagnostics: write/read probes never rely on opendir/access checks.
    void refresh_storage_directory_status(){
        auto inspect=[](const char *folder,bool &visible,int &error){
            DIR *dir=opendir(folder);
            if(dir){closedir(dir);visible=true;error=0;}
            else {visible=false;error=errno;}
        };
        inspect(kPrimaryStateFolder,primary_storage_visible_,primary_storage_error_);
        inspect(kFallbackStateFolder,fallback_storage_visible_,fallback_storage_error_);
    }
    void run_storage_probe(const char *folder,const char *filename,
                           ulg::StorageStatus &write_status,ulg::StorageStatus &read_status,
                           std::string &cleanup_status){
        write_status={};read_status={};cleanup_status="not_run";
        const std::string path=std::string(folder)+"/"+filename;
        FILE *existing=std::fopen(path.c_str(),"rb");
        if(existing){std::fclose(existing);write_status.stage="probe_file_exists";return;}
        if(errno!=ENOENT){write_status.stage="check_probe_path";write_status.os_error=errno;return;}
        existing=std::fopen((path+".tmp").c_str(),"rb");
        if(existing){std::fclose(existing);write_status.stage="probe_tmp_exists";return;}
        if(errno!=ENOENT){write_status.stage="check_probe_tmp";write_status.os_error=errno;return;}
        ulg::Preferences expected;
        expected.recent.push_back(std::string("PATH|BGH-DIAG|")+filename);
        if(ulg::write_preferences(expected,path,&write_status)){
            ulg::Preferences actual;
            if(ulg::read_preferences(path,actual,&read_status) && actual.recent!=expected.recent){
                read_status.ok=false;read_status.stage="data_mismatch";
            }
            if(std::remove(path.c_str())==0)cleanup_status="removed";
            else cleanup_status="remove_errno_"+std::to_string(errno);
        }
    }
    void probe_recent_storage(){
        refresh_storage_directory_status();
        run_storage_probe(kPrimaryStateFolder,"storage_probe_v126_primary.bin",
            probe_primary_write_status_,probe_primary_read_status_,probe_primary_cleanup_status_);
        run_storage_probe(kFallbackStateFolder,"storage_probe_v126_fallback.bin",
            probe_fallback_write_status_,probe_fallback_read_status_,probe_fallback_cleanup_status_);
    }
    static std::string storage_status_label(const ulg::StorageStatus &status){
        return (status.ok?"OK/":"FAIL/")+status.stage+
            (status.os_error?" errno="+std::to_string(status.os_error):"");
    }
    // Return whether at least one writable location persisted the current BGH state.
    bool save_prefs(){
        ulg::Preferences primary;primary.favorites=favorites_;primary.recent=recent_list_;
        ulg::Preferences backup;
        for(const auto &token:recent_list_){
            std::string key;
            if(token.rfind("PATH|",0)==0)key=token;
            else for(const auto &g:catalogue_.games)if(g.id==token){
                key="PATH|"+g.platform+"|"+g.path;break;
            }
            if(!key.empty() && std::find(backup.recent.begin(),backup.recent.end(),key)==backup.recent.end())
                backup.recent.push_back(std::move(key));
        }
        auto save_location=[&](const char *prefs_path,const char *recent_path,
                               ulg::StorageStatus &prefs_status,ulg::StorageStatus &recent_status){
            const bool saved_main=ulg::write_preferences(primary,prefs_path,&prefs_status);
            // Always write the backup, including an empty list: a refused
            // first-ever launch must not leave its staged token on disk.
            const bool saved_backup=ulg::write_preferences(backup,recent_path,&recent_status);
            return saved_main || saved_backup;
        };
        const bool saved_primary=save_location(kPrimaryPrefsFile,kPrimaryRecentBackupFile,
            prefs_write_status_,recent_write_status_);
        const bool saved_fallback=save_location(kFallbackPrefsFile,kFallbackRecentBackupFile,
            fallback_prefs_write_status_,fallback_recent_write_status_);
        recent_save_error_=!(saved_primary || saved_fallback);
        return saved_primary || saved_fallback;
    }
    void record_recent(const std::string &id){
        // A pre-launch write is only a *pending* start request, not backend acceptance.
        const ulg::Game* game=nullptr;
        for(const auto &candidate:catalogue_.games)if(candidate.id==id){game=&candidate;break;}
        if(!game){recent_save_error_=true;return;}
        const std::string stable="PATH|"+game->platform+"|"+game->path;
        // Remove both old ID and path aliases, then move this game to the front.
        recent_list_.erase(std::remove_if(recent_list_.begin(),recent_list_.end(),
            [this,game](const std::string &token){return recent_matches(*game,token);}),recent_list_.end());
        recent_list_.insert(recent_list_.begin(),stable);
        if(recent_list_.size()>80)recent_list_.resize(80);
        recent_.clear();for(const auto &token:recent_list_)recent_.insert(token);
        (void)save_prefs(); // Complete the local write BEFORE contacting the launcher.
    }
    void rollback_recent(const std::vector<std::string> &previous_recent){
        launch_request_accepted_=false;
        recent_list_=previous_recent;
        recent_.clear();
        for(const auto &token:recent_list_)recent_.insert(token);
        // Persist the restored history so a refused request leaves no false entry.
        (void)save_prefs();
    }
    bool recent_matches(const ulg::Game &g,const std::string &token) const {
        return token==g.id || token==("PATH|"+g.platform+"|"+g.path);
    }
    bool game_in_recent(const ulg::Game &g) const {
        if(recent_.count(g.id)>0)return true;
        for(const auto &token:recent_list_)if(recent_matches(g,token))return true;
        return false;
    }
    // Use a pre-rasterized serif atlas for dynamic titles. The PlayStation glyphs
    // and all controller/navigation actions remain handled by the UI toolkit.
    void serif_text(gfx::DrawList &list,const std::string &value,float x,float baseline,
                    float size,Color tint,float max_width=0.0f) const {
        const std::string display=ulg::translate_ui(value,language_settings_.english);
        const auto tex=covers_.get("/app0/assets/ui/serif_atlas.png");
        if(!tex){translated_text(list,context_.fonts.regular,display,x,baseline,size,tint);return;}
        const float scale=size/ulg::kSerifAtlasEm;
        float cursor=x;
        for(std::size_t at=0;at<display.size();){
            const unsigned char first=static_cast<unsigned char>(display[at++]);
            std::uint32_t cp=first;
            if((first&0xE0)==0xC0 && at<display.size())
                cp=((first&0x1F)<<6)|(static_cast<unsigned char>(display[at++])&0x3F);
            else if((first&0xF0)==0xE0 && at+1<display.size()){
                cp=((first&0x0F)<<12)|((static_cast<unsigned char>(display[at++])&0x3F)<<6);
                cp|=static_cast<unsigned char>(display[at++])&0x3F;
            }else if((first&0xF8)==0xF0 && at+2<display.size()){
                at+=3;cp='?';
            }
            const ulg::SerifAtlasGlyph *g=nullptr;
            for(const auto &item:ulg::kSerifAtlasGlyphs)if(item.cp==cp){g=&item;break;}
            if(!g){for(const auto &item:ulg::kSerifAtlasGlyphs)if(item.cp=='?'){g=&item;break;}}
            if(!g)continue;
            if(max_width>0.0f && cursor+g->advance*scale>x+max_width)break;
            list.image(tex,{cursor,baseline-ulg::kSerifAtlasBaseline*scale,g->w*scale,g->h*scale},
                {g->x/ulg::kSerifAtlasW,g->y/ulg::kSerifAtlasH,g->w/ulg::kSerifAtlasW,g->h/ulg::kSerifAtlasH},tint,0);
            cursor+=g->advance*scale;
        }
    }
    float serif_text_width(const std::string &value,float size) const {
        // Identical advance metrics and UTF-8 parsing as serif_text(),
        // so capsules receive genuinely symmetric left/right padding.
        const float scale=size/ulg::kSerifAtlasEm;
        float width=0.0f;
        for(std::size_t at=0;at<value.size();){
            const unsigned char first=static_cast<unsigned char>(value[at++]);
            std::uint32_t cp=first;
            if((first&0xE0)==0xC0 && at<value.size())
                cp=((first&0x1F)<<6)|(static_cast<unsigned char>(value[at++])&0x3F);
            else if((first&0xF0)==0xE0 && at+1<value.size()){
                cp=((first&0x0F)<<12)|((static_cast<unsigned char>(value[at++])&0x3F)<<6);
                cp|=static_cast<unsigned char>(value[at++])&0x3F;
            }else if((first&0xF8)==0xF0 && at+2<value.size()){
                at+=3;cp='?';
            }
            const ulg::SerifAtlasGlyph *glyph=nullptr;
            for(const auto &item:ulg::kSerifAtlasGlyphs)if(item.cp==cp){glyph=&item;break;}
            if(!glyph)for(const auto &item:ulg::kSerifAtlasGlyphs)if(item.cp=='?'){glyph=&item;break;}
            if(glyph)width+=glyph->advance*scale;
        }
        return width;
    }
    void validate_cached_roots(){
        for(std::size_t i=0;i<catalogue_.roots.size();++i){
            catalogue_.roots[i]=ulg::probe_root(roots_[i]);
        }
        catalogue_.failed_operations=0;catalogue_.failed_roots=0;catalogue_.disabled_roots=0;
        for(const auto &root:catalogue_.roots){
            if(root.status=="deaktiviert")++catalogue_.disabled_roots;
            else if(root.status!="ok"){
                ++catalogue_.failed_roots;++catalogue_.failed_operations;
            }
        }
        for(auto &g:catalogue_.games){
            struct stat game_stat{};
            g.available=g.via_ftp || (lstat(g.path.c_str(),&game_stat)==0 &&
                (S_ISREG(game_stat.st_mode)||S_ISDIR(game_stat.st_mode)));
            for(const auto &r:catalogue_.roots){
                if(r.status!="ok" && r.status!="ftp_ok" && g.path.size()>r.path.size() &&
                   g.path.compare(0,r.path.size(),r.path)==0 && g.path[r.path.size()]=='/'){
                    g.available=false;break;
                }
            }
        }
    }
    // Return true if an expensive decode/import was attempted during this frame.
    // Recent and visible games deliberately share the exact same cover pipeline.
    bool load_cover_for_game(ulg::Game &g){
        if(g.available && g.cover_path.empty() &&
           (g.platform=="PS2" || g.platform=="Switch" || g.platform=="Xbox 360") &&
           attempted_emulator_covers_.insert(g.id).second){
            ulg::EmulatorCoverImporter importer(kEmulatorCoverFolder);
            std::string diagnosis;
            if(importer.hydrate(g,&diagnosis)){
                cover_diagnostics_[g.id]=diagnosis;
                (void)ulg::write_catalogue_cache(catalogue_,kCacheFile);
            } else cover_diagnostics_[g.id]=diagnosis;
            return true;
        }
        if(!g.cover_path.empty() && !covers_.has(g.cover_path)){
            const std::string failed_path=g.cover_path;
            if(!covers_.load(failed_path)){
                // Do not delete anything on a GPU upload error.
                cover_diagnostics_[g.id]=covers_.error(failed_path);
                if(covers_.decode_failed(failed_path) &&
                   failed_path.compare(0,std::strlen(kEmulatorCoverFolder),kEmulatorCoverFolder)==0 &&
                   failed_path.size()>std::strlen(kEmulatorCoverFolder) &&
                   bad_cover_reimports_[g.id]++==0){
                    (void)std::remove(failed_path.c_str()); // only BGH-owned cache
                    g.cover_path.clear();
                    attempted_emulator_covers_.erase(g.id);
                    cover_diagnostics_[g.id]="BGH CACHE BILD DEFEKT; NEUER IMPORT";
                }
            }else cover_diagnostics_[g.id]="BILD/GL TEXTUR OK";
            return true;
        }
        return false;
    }
    void load_next_visible_cover(){
        for(const char* name:{"ps5","ps4","ps2","switch","xbox360"}){
            const std::string path=std::string("/app0/assets/platforms/")+name+".png";
            if(!covers_.has(path)){covers_.load(path);return;}
        }
        for(const char* name:{"grid","gamepad","emulator","star","clock","search","settings","controller","cover_shadow","cover_frame","cover_frame_active","fan_case_header_ps5","fan_case_header_ps4","fan_case_header_switch"}){
            const std::string path=std::string("/app0/assets/ui/")+name+".png";
            if(!covers_.has(path)){covers_.load(path);return;}
        }
        // These are catalogue indices and do NOT depend on active filters or
        // visible list positions. Reloaded after refresh() clears the texture cache.
        for(const int index:recent_indices()){
            if(index<0 || index>=static_cast<int>(catalogue_.games.size()))continue;
            if(load_cover_for_game(catalogue_.games[static_cast<std::size_t>(index)]))return;
        }
        if(indices_.empty())return;
        // During fast navigation give the hero and its immediate neighbours an
        // unconditional chance to load, but still do no more than ONE costly
        // import/decode/upload per frame. Recent cards and UI assets stay first.
        for(const int offset:{0,-1,1}){
            const int position=selected_+offset;
            if(position<0 || position>=static_cast<int>(indices_.size()))continue;
            auto &g=catalogue_.games[indices_[static_cast<std::size_t>(position)]];
            if(load_cover_for_game(g))return;
        }
        // Distant fan/list artwork is deferred only during active navigation.
        if(!cover_load_debounce_.allow_regular_covers())return;
        // Active/nearest entries are cached now, so prioritize remaining fan
        // neighbours and the visible list while keeping the existing ordering.
        std::array<std::size_t,24> priorities{};
        std::size_t priority_count=0;
        const auto enqueue=[&](int idx){
            if(idx<0 || idx>=static_cast<int>(indices_.size()))return;
            const auto id=static_cast<std::size_t>(idx);
            for(std::size_t n=0;n<priority_count;++n)if(priorities[n]==id)return;
            if(priority_count<priorities.size())priorities[priority_count++]=id;
        };
        enqueue(selected_);
        for(int distance=1;distance<=3;++distance){
            enqueue(selected_-distance);
            enqueue(selected_+distance);
        }
        const int first=std::max(0,static_cast<int>(list_scroll_)-1);
        for(int i=first;i<std::min(static_cast<int>(indices_.size()),first+kVisibleListRows+3);++i)
            enqueue(i);
        for(std::size_t pos=0;pos<priority_count;++pos){
            auto &g=catalogue_.games[indices_[priorities[pos]]];
            if(load_cover_for_game(g))return;
        }
    }
    void refresh() {
        const std::string selected_id=(!indices_.empty() && selected_<static_cast<int>(indices_.size())) ?
            catalogue_.games[indices_[static_cast<std::size_t>(selected_)]].id : "";
        auto fresh=ulg::scan(roots_,5,5000);
        // Existing localhost ftpsrv is an optional read-only fallback; no payload started.
        ulg::supplement_from_local_ftp(fresh,roots_,5,5000,2121,"/download0/hui/ulg_v04/covers");
        catalogue_=ulg::merge_unavailable(std::move(fresh),catalogue_);
        apply_presentation();
        ulg::clean_catalogue(catalogue_,roots_);
        (void)ulg::write_catalogue_cache(catalogue_,kCacheFile);
        covers_.clear();
        cover_load_debounce_.reset();
        attempted_emulator_covers_.clear();cover_diagnostics_.clear();bad_cover_reimports_.clear();
        ++refresh_count_;rebuild();
        if(!selected_id.empty())for(std::size_t i=0;i<indices_.size();++i)
            if(catalogue_.games[indices_[i]].id==selected_id){selected_=static_cast<int>(i);break;}
        snap_navigation();
    }
    static constexpr const char *kSearchKeys="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 <-";
    void update_search(const InputFrame &input,app::Feedback &feedback){
        constexpr int columns=9;
        const int count=static_cast<int>(std::strlen(kSearchKeys));
        int next=search_key_;
        if(input.nav==Direction::left)next-=1;
        if(input.nav==Direction::right)next+=1;
        if(input.nav==Direction::up)next-=columns;
        if(input.nav==Direction::down)next+=columns;
        next=std::clamp(next,0,count-1);
        if(next!=search_key_){search_key_=next;feedback.play(audio::Cue::focus);}
        if(input.is_pressed(Action::west) || input.is_pressed(Action::north)){
            if(!search_query_.empty())search_query_.pop_back();
            rebuild();
        }
        if(input.is_pressed(Action::confirm)){
            const char key=kSearchKeys[search_key_];
            if(key=='<'){if(!search_query_.empty())search_query_.pop_back();}
            else if(key=='-'){search_open_=false;}
            else if(search_query_.size()<48)search_query_+=key;
            rebuild();feedback.play(audio::Cue::select);
        }
    }
    void draw_search(app::Frame &frame) const {
        frame.glass=true;
        auto &list=frame.overlay;const auto &fonts=context_.fonts;
        list.rounded_rect({327,160,1268,754},32,Color::rgb(0x151D2D,0.98f));
        translated_text(list,fonts.display,"Spiele suchen",385,245,45,kInk);
        list.rounded_rect({385,277,1150,80},16,Color::rgb(0x26314C));
        translated_text(list,fonts.semibold,search_query_.empty()?"Titel eingeben ...":search_query_,415,328,31,kInk);
        const int count=static_cast<int>(std::strlen(kSearchKeys));
        for(int i=0;i<count;++i){
            const int col=i%9,row=i/9;
            const float x=391+col*126.0f,y=395+row*95.0f;
            list.rounded_rect({x,y,109,74},13,i==search_key_?Color::rgb(0x596DBA):Color::rgb(0x273146));
            const char letter=kSearchKeys[i];
            std::string caption=letter==' '?"LEER":(letter=='<'?"LOESCHEN":(letter=='-'?"FERTIG":std::string(1,letter)));
            translated_text(list,fonts.semibold,caption,x+13,y+48,letter==' '||letter=='<'||letter=='-'?16:27,kInk);
        }
        translated_text(list,fonts.regular,"X: Buchstabe    Dreieck: Loeschen    Kreis: Fertig",400,867,22,kMuted);
    }
    struct SidebarItem {
        std::string id,label,icon;
        int legacy_icon=6;
        bool is_platform=false;
    };
    std::string active_sidebar_id() const {
        if(tab_>=0 && tab_<static_cast<int>(sidebar_items_.size()))
            return sidebar_items_[static_cast<std::size_t>(tab_)].id;
        return "Alle Spiele";
    }
    void rebuild_sidebar(){
        const std::string keep=active_sidebar_id();
        std::vector<SidebarItem> items;
        items.push_back({"Alle Spiele","Alle Spiele","",0,false});
        const auto order=ulg::platform_order(platform_registry_);
        for(const auto index:order){
            const auto &p=platform_registry_.entries[index];
            if(!ulg::sidebar_profile_visible(p))continue;
            int legacy_icon=6;
            if(p.id=="PS5")legacy_icon=1;
            else if(p.id=="PS4")legacy_icon=2;
            else if(p.id=="PS2")legacy_icon=3;
            else if(p.id=="Switch")legacy_icon=4;
            else if(p.id=="Xbox 360")legacy_icon=5;
            items.push_back({p.id,p.short_label,p.icon_path,legacy_icon,true});
        }
        items.push_back({"Emulatoren","Emulatoren","",6,false});
        items.push_back({"Favoriten","Favoriten","",7,false});
        items.push_back({"Zuletzt","Zuletzt","",8,false});
        sidebar_items_=std::move(items);
        tab_=0;
        for(std::size_t i=0;i<sidebar_items_.size();++i)
            if(sidebar_items_[i].id==keep){tab_=static_cast<int>(i);break;}
    }
    bool persist_platform_registry(ulg::PlatformRegistry candidate){
        if(candidate.revision==UINT32_MAX){platform_settings_message_="Maximale Revision erreicht";return false;}
        ++candidate.revision;
        const bool primary=ulg::write_platform_registry(candidate,kPrimaryPlatformFile);
        const bool fallback=ulg::write_platform_registry(candidate,kFallbackPlatformFile);
        if(!primary && !fallback){platform_settings_message_="SPEICHERFEHLER: Plattformen unveraendert";return false;}
        platform_registry_=std::move(candidate);
        platform_settings_message_="Plattformanordnung gespeichert";
        rebuild_sidebar();rebuild();
        return true;
    }
    // One registry and one global order for native PS4/PS5 AND emulators.
    std::vector<std::size_t> emulator_profile_indices() const {
        return ulg::platform_order(platform_registry_);
    }
    // Read metadata only on startup or when user saves an app path, not once
    // per video frame. Existing verified launch backends are unaffected.
    void refresh_emulator_detections(){
        emulator_profiles_=ulg::emulator_detection::load_profiles();
        detected_emulators_.clear();
        // POSIX lstat on a different title's /data/homebrew/ directory can be
        // blocked by the PS5 app sandbox although an existing FTP daemon sees it.
        // Reuse only the same read-only loopback FTP service as the game scanner.
        ulg::FtpReadonly ftp;
        for(const auto &p:platform_registry_.entries)if(p.is_emulator){
            auto detected=ulg::emulator_detection::detect(p.app_path,p.id,emulator_profiles_);
            if(!p.app_path.empty() &&
               (detected.status==ulg::emulator_detection::Detection::Status::invalid_path ||
                detected.status==ulg::emulator_detection::Detection::Status::incomplete) &&
               ulg::safe_platform_path(p.app_path)){
                const auto from_ftp=ulg::emulator_detection::detect_via_ftp(
                    p.app_path,p.id,emulator_profiles_,ftp);
                // Prefer actual FTP status for sandbox-inaccessible folders.
                // Incomplete local metadata can still be more informative than
                // an unreachable FTP service.
                if(detected.status==ulg::emulator_detection::Detection::Status::invalid_path ||
                   (from_ftp.status!=ulg::emulator_detection::Detection::Status::invalid_path &&
                    from_ftp.status!=ulg::emulator_detection::Detection::Status::ftp_unavailable))
                    detected=from_ftp;
            }
            detected_emulators_[p.id]=std::move(detected);
        }
    }
    const ulg::emulator_detection::Detection &emulator_detection_for(const std::string &platform) const {
        static const ulg::emulator_detection::Detection missing{};
        const auto it=detected_emulators_.find(platform);
        return it==detected_emulators_.end()?missing:it->second;
    }
    std::string application_name(const ulg::PlatformEntry &p) const {
        const auto &d=emulator_detection_for(p.id);
        if(!p.app_path.empty())return ulg::emulator_detection::description(d);
        if(p.id=="PS2")return "PS5SX2";
        if(p.id=="Switch")return "ProsperoEden";
        if(p.id=="Xbox 360")return "PS5X360";
        if(p.id=="PS5" || p.id=="PS4")return "Installierte Systemanwendung";
        if(p.scan_supported)return "Scanner ja / Start offen";
        return "Vorlage / kein Backend";
    }
    void test_emulator_menu(const ulg::PlatformEntry &p,app::Feedback &feedback){
        const auto &d=emulator_detection_for(p.id);
        if(d.status!=ulg::emulator_detection::Detection::Status::known_emulator){
            platform_settings_message_="Kein bekanntes Emulatorprofil / Menue nicht freigegeben";
            feedback.play(audio::Cue::error);return;
        }
        // Explicit Square key: no ROM arguments and no helper ELF execution.
        // A queued launch is an experiment, not proof of PS5 compatibility.
        ulg::LaunchPlan plan{};
        plan.target=d.title_id;plan.supported=true;
        const auto result=ulg::send_launch(plan);
        platform_settings_message_=result.message;
        if(result.status==ulg::LaunchStatus::started || result.status==ulg::LaunchStatus::queued){
            feedback.play(audio::Cue::launch);
            if(result.status==ulg::LaunchStatus::queued)sys::quit();
        }else feedback.play(audio::Cue::error);
    }
    void begin_emulator_path_editor(bool app_path,std::size_t folder,bool manual){
        emulator_path_edit_=true;
        emulator_edit_app_=app_path;
        emulator_edit_folder_=folder; // index == size means add a new root
        edited_root_=-1;
        const auto profiles=emulator_profile_indices();
        const auto &p=platform_registry_.entries[profiles[static_cast<std::size_t>(emulator_selected_)]];
        input_path_=app_path?(p.app_path.empty()?"/":p.app_path):
            (folder<p.rom_folders.size()?p.rom_folders[folder]:"/");
        if(manual){path_key_=0;path_editor_open_=true;}
        else {browser_open_=true;browser_path_="/";browser_selected_=0;refresh_browser();}
    }
    void remove_emulator_folder(std::size_t index){
        const auto profiles=emulator_profile_indices();
        auto candidate=platform_registry_;
        auto &profile=candidate.entries[profiles[static_cast<std::size_t>(emulator_selected_)]];
        if(index>=profile.rom_folders.size())return;
        const std::string removing=profile.rom_folders[index];
        if(profile.scan_supported){
            auto next_roots=roots_;
            next_roots.erase(std::remove_if(next_roots.begin(),next_roots.end(),[&](const ulg::Root &root){
                return root.platform==profile.id && root.path==removing;
            }),next_roots.end());
            if(!ulg::save_user_roots(next_roots,kUserPathsFile)){
                platform_settings_message_="SPEICHERFEHLER: Spieleordner unveraendert";return;
            }
            roots_=std::move(next_roots);
        }
        (void)ulg::remove_profile_folder(profile,index);
        if(persist_platform_registry(std::move(candidate))){
            emulator_subselected_=std::min(emulator_subselected_,
                static_cast<int>(platform_registry_.entries[profiles[static_cast<std::size_t>(emulator_selected_)]].rom_folders.size())+1);
            if(platform_registry_.entries[profiles[static_cast<std::size_t>(emulator_selected_)]].scan_supported)refresh();
            platform_settings_message_="Nur Ordner-Verknuepfung entfernt (keine Dateien geloescht)";
        }
    }
    // Preserve V1.1.2's per-folder scan enable switch after retiring the
    // duplicate folder tab. This only updates BGH's own user_paths file.
    bool toggle_emulator_folder_scan(const ulg::PlatformEntry &profile,std::size_t folder){
        if(!profile.scan_supported || folder>=profile.rom_folders.size())return false;
        auto next=roots_;
        for(auto &root:next){
            if(root.platform==profile.id && root.path==profile.rom_folders[folder]){
                root.enabled=!root.enabled;
                if(!ulg::save_user_roots(next,kUserPathsFile)){
                    platform_settings_message_="SPEICHERFEHLER: Ordnerstatus unveraendert";
                    return false;
                }
                roots_=std::move(next);
                refresh();
                platform_settings_message_="Scanordner AN/AUS gespeichert";
                return true;
            }
        }
        platform_settings_message_="Dieser Pfad hat noch keinen aktiven Scanner";
        return false;
    }
    bool profile_folder_enabled(const ulg::PlatformEntry &profile,std::size_t folder) const {
        if(folder>=profile.rom_folders.size())return false;
        for(const auto &root:roots_)
            if(root.platform==profile.id && root.path==profile.rom_folders[folder])
                return root.enabled;
        return false;
    }
    void update_emulator_settings(const InputFrame &input,app::Feedback &feedback){
        auto profiles=emulator_profile_indices();if(profiles.empty())return;
        emulator_selected_=std::clamp(emulator_selected_,0,static_cast<int>(profiles.size())-1);
        const auto index=profiles[static_cast<std::size_t>(emulator_selected_)];
        const auto &profile=platform_registry_.entries[index];
        if(emulator_expanded_ && emulator_subselected_==0 && input.is_pressed(Action::west)){
            test_emulator_menu(profile,feedback);return;
        }
        if(input.is_pressed(Action::west)){
            emulator_expanded_=!emulator_expanded_;
            emulator_subselected_=-1;
            feedback.play(audio::Cue::open);return;
        }
        if(input.is_pressed(Action::jump_prev)||input.is_pressed(Action::jump_next)){
            auto candidate=platform_registry_;
            if(ulg::move_platform(candidate,profile.id,input.is_pressed(Action::jump_next)?1:-1)){
                if(persist_platform_registry(std::move(candidate))){
                    profiles=emulator_profile_indices();
                    for(std::size_t n=0;n<profiles.size();++n)
                        if(platform_registry_.entries[profiles[n]].id==profile.id){
                            emulator_selected_=static_cast<int>(n);break;
                        }
                }
                feedback.play(audio::Cue::select);
            }
            return;
        }
        const int children=static_cast<int>(profile.rom_folders.size())+2; // app row + folders + add
        if(input.nav==Direction::up){
            if(emulator_expanded_ && emulator_subselected_>=0) --emulator_subselected_;
            else {emulator_selected_=std::max(0,emulator_selected_-1);emulator_subselected_=-1;
                emulator_expanded_=false;}
            feedback.play(audio::Cue::focus);return;
        }
        if(input.nav==Direction::down){
            if(emulator_expanded_ && emulator_subselected_<children-1) ++emulator_subselected_;
            else {emulator_selected_=std::min(static_cast<int>(profiles.size())-1,emulator_selected_+1);
                emulator_subselected_=-1;emulator_expanded_=false;}
            feedback.play(audio::Cue::focus);return;
        }
        if(emulator_expanded_ && emulator_subselected_>=0){
            const bool app=emulator_subselected_==0;
            const std::size_t folder=static_cast<std::size_t>(std::max(0,emulator_subselected_-1));
            if(input.is_pressed(Action::north) && !app && folder<profile.rom_folders.size()){
                remove_emulator_folder(folder);feedback.play(audio::Cue::select);return;
            }
            if(input.is_pressed(Action::l3) && !app && folder<profile.rom_folders.size()){
                feedback.play(toggle_emulator_folder_scan(profile,folder)?
                    audio::Cue::select:audio::Cue::error);
                return;
            }
            if(input.is_pressed(Action::confirm)){
                begin_emulator_path_editor(app,folder,true);
                feedback.play(audio::Cue::open);return;
            }
            if(input.is_pressed(Action::touch)){
                begin_emulator_path_editor(app,folder,true);
                feedback.play(audio::Cue::open);return;
            }
            return;
        }
        if(input.is_pressed(Action::confirm)){
            auto candidate=platform_registry_;
            candidate.entries[index].enabled=!candidate.entries[index].enabled;
            feedback.play(persist_platform_registry(std::move(candidate))?
                audio::Cue::select:audio::Cue::error);
            return;
        }
        if(input.is_pressed(Action::north)){
            emulator_expanded_=true;emulator_subselected_=children-1; // '+ Ordner'
            begin_emulator_path_editor(false,profile.rom_folders.size(),true);
            feedback.play(audio::Cue::open);return;
        }
    }
    void save_emulator_path(const std::string &path){
        const auto profiles=emulator_profile_indices();
        if(profiles.empty() || emulator_selected_<0 || emulator_selected_>=static_cast<int>(profiles.size()))return;
        const auto index=profiles[static_cast<std::size_t>(emulator_selected_)];
        if(!ulg::safe_platform_path(path) || path.empty()){
            platform_settings_message_="Ungueltiger Ordnerpfad";return;
        }
        auto candidate=platform_registry_;
        auto &profile=candidate.entries[index];
        if(emulator_edit_app_){
            profile.app_path=path; // informational only: launch still uses known title IDs
        }else{
            const bool existing=emulator_edit_folder_<profile.rom_folders.size();
            const std::string old=existing?profile.rom_folders[emulator_edit_folder_]:std::string();
            if(existing && old==path){browser_open_=false;path_editor_open_=false;return;}
            if(existing){
                (void)ulg::remove_profile_folder(profile,emulator_edit_folder_);
            }
            if(!ulg::add_profile_folder(profile,path)){
                platform_settings_message_="Maximal 16 verschiedene Spielordner";return;
            }
            if(profile.scan_supported){
                auto next_roots=roots_;
                bool previously_enabled=true;
                if(existing){
                    for(const auto &r:next_roots)
                        if(r.platform==profile.id && r.path==old)previously_enabled=r.enabled;
                    next_roots.erase(std::remove_if(next_roots.begin(),next_roots.end(),[&](const ulg::Root &r){
                        return r.platform==profile.id && r.path==old;
                    }),next_roots.end());
                }
                bool found=false;
                for(const auto &r:next_roots)if(r.platform==profile.id && r.path==path)found=true;
                if(!found && next_roots.size()<128)next_roots.push_back({profile.id,path,previously_enabled});
                if((!found && next_roots.size()>=128) ||
                   !ulg::save_user_roots(next_roots,kUserPathsFile)){
                    platform_settings_message_="Scanpfad konnte nicht gespeichert werden";return;
                }
                roots_=std::move(next_roots);
            }
        }
        if(persist_platform_registry(std::move(candidate))){
            if(emulator_edit_app_){
                refresh_emulator_detections();
                platform_settings_message_=ulg::emulator_detection::description(
                    emulator_detection_for(platform_registry_.entries[index].id));
            }
            if(!emulator_edit_app_ && platform_registry_.entries[index].scan_supported)refresh();
            else if(!emulator_edit_app_)
                platform_settings_message_="Pfad vorgemerkt; fuer diese Plattform noch kein Scanner";
        }
        emulator_path_edit_=false;browser_open_=false;path_editor_open_=false;
    }
    void switch_tab(int new_tab){
        cover_load_debounce_.reset();
        launch_in_progress_=false;
        tab_=std::clamp(new_tab,0,std::max(0,static_cast<int>(sidebar_items_.size())-1));
        rebuild();
        snap_navigation();
    }
    void snap_navigation(){
        visual_selection_=static_cast<float>(selected_);
        list_scroll_=ulg::premium::scroll_target(selected_,indices_.size(),kVisibleListRows);
    }
    void rebuild(){
        const std::string keep=(!indices_.empty() && selected_>=0 &&
            selected_<static_cast<int>(indices_.size()) && indices_[selected_]<catalogue_.games.size())
            ?catalogue_.games[indices_[static_cast<std::size_t>(selected_)]].id : std::string();
        indices_.clear();
        counts_.fill(0);
        for(std::size_t i=0;i<catalogue_.games.size();++i){
            const auto &g=catalogue_.games[i];
            // The total counts games only. Emulator apps belong exclusively to
            // their own tab and do not inflate the game library count.
            if(g.platform!="Homebrew")++counts_[0];
            for(std::size_t c=1;c<=6;++c)if(g.platform==(c==6?"Homebrew":kTabs[c]))++counts_[c];
            if(favorites_.count(g.id)>0)++counts_[7];
            if(game_in_recent(g))++counts_[8];
            const std::string view=active_sidebar_id();
            bool show=(view=="Alle Spiele" && g.platform!="Homebrew") ||
                (view=="Emulatoren" && g.platform=="Homebrew") ||
                (view==g.platform && view!="Homebrew") ||
                (view=="Favoriten" && favorites_.count(g.id)>0) ||
                (view=="Zuletzt" && game_in_recent(g));
            if(show && !search_query_.empty()){
                auto name=g.title;
                for(char &c:name)if(c>='a' && c<='z')c=static_cast<char>(c-'a'+'A');
                show=name.find(search_query_)!=std::string::npos;
            }
            if(show)indices_.push_back(i);
        }
        selected_=0;
        if(!keep.empty())for(std::size_t i=0;i<indices_.size();++i)
            if(catalogue_.games[indices_[i]].id==keep){selected_=static_cast<int>(i);break;}
        snap_navigation();
    }
    void toggle_favorite(){
        if(indices_.empty())return;
        auto &id=catalogue_.games[indices_[static_cast<std::size_t>(selected_)]].id;
        if(!favorites_.insert(id).second)favorites_.erase(id);
        save_prefs();rebuild();
    }
    void draw_game_list(gfx::DrawList &list) const {
        const auto &fonts=context_.fonts;
        if(indices_.empty()){
            translated_text(list,fonts.regular,"Keine Einträge",329,305,24,kMuted);return;
        }
        const int first=std::max(0,static_cast<int>(std::floor(list_scroll_))-1);
        const int last=std::min(static_cast<int>(indices_.size()),first+kVisibleListRows+3);
        for(int i=first;i<last;++i){
            const float y=kListTop+(static_cast<float>(i)-list_scroll_)*kListRowH;
            if(y<kListTop-1 || y+kListRowH>kListTop+kVisibleListRows*kListRowH+2)continue;
            const auto &g=catalogue_.games[indices_[static_cast<std::size_t>(i)]];
            const bool active=i==selected_;
            if(active){
                list.glow({314,y+1,370,72},16,12,Color::rgb(0x429688,0.24f));
                list.rounded_rect({314,y+1,367,71},16,Color::rgb(0x1B4443,0.42f));
                list.bordered_rect({314,y+1,367,71},16,Color::rgb(0xFFFFFF,0),1.5f,
                    Color::rgb(0x7CE0C8,0.65f));
            }
            const auto tex=covers_.get(g.cover_path);
            if(tex && g.available)list.image(tex,{326,y+8,57,57},gfx::kFullUv,Color::rgb(0xFFFFFF),7);
            else{
                list.rounded_rect({326,y+8,57,57},10,platform_color(g.platform).with_alpha(0.22f));
                translated_text(list,fonts.semibold,shorten(g.platform,5),329,y+39,13,Color::rgb(0xEFE3D2));
            }
            serif_text(list,shorten(g.title,24),399,y+31,22,active?kInk:Color::rgb(0xEEE4D7),271);
            serif_text(list,g.available?g.platform:g.platform+" • offline",399,y+58,17,active?Color::rgb(0xEBDCBF):Color::rgb(0xD3CABD),270);
        }
        if(indices_.size()>kVisibleListRows){
            list.rounded_rect({685,186,3,707},2,Color::rgb(0xDBC9A2,0.35f));
            const float thumb=std::max(28.0f,707.0f*static_cast<float>(kVisibleListRows)/indices_.size());
            const float pos=static_cast<float>(std::max(0.0f,list_scroll_)) /
                std::max(1.0f,static_cast<float>(indices_.size()-kVisibleListRows));
            list.rounded_rect({685,186+pos*(707-thumb),3,thumb},2,Color::rgb(0xCFD9B9));
        }
    }
    void draw_fan_cover(gfx::DrawList &list,int index) const {
        if(index<0 || index>=static_cast<int>(indices_.size()))return;
        const auto &g=catalogue_.games[indices_[static_cast<std::size_t>(index)]];
        const float relative=static_cast<float>(index)-visual_selection_;
        const float depth=std::abs(relative);
        if(depth>3.2f)return;
        // Front cover has a stronger hero emphasis; all sizes animate with selection.
        const float base_scale=std::max(0.50f,1.0f-depth*0.20f);
        const float hero_boost=0.20f*std::max(0.0f,1.0f-depth*0.88f);
        const float scale=base_scale+hero_boost;
        const float w=308*scale;
        float h=421*scale;
        // The decoded source determines the aspect ratio of PS2/Xbox art.
        // Synthetic PS4/PS5/Switch cases keep their validated fixed layout.
        const bool with_case_header=fan_cover_uses_case_header(g.platform);
        const auto cover_dimensions=covers_.size(g.cover_path);
        if(!with_case_header && covers_.has(g.cover_path))
            h=fan_native_card_height(cover_dimensions.first,cover_dimensions.second,w,h);
        // V1.0.0 uses a tighter five-cover hero fan: larger main cover, wider wings.
        const float near_distance=std::min(depth,1.0f)*270.0f;
        const float far_distance=std::max(0.0f,depth-1.0f)*194.0f;
        const float distance=near_distance+far_distance;
        const float cx=kFanCentreX+std::copysign(distance,relative);
        const float cy=kFanCentreY+depth*24.0f;
        const gfx::Rect card{cx-w*0.5f,cy-h*0.5f,w,h};
        // Studio contrast boost: a soft local dimmer behind the hero artwork.
        // Applied before the cover so it never darkens the actual cover pixels.
        if(depth<0.60f)
            list.shadow({card.x-30.0f,card.y-20.0f,card.w+60.0f,card.h+40.0f},
                36.0f,28.0f,Color::rgb(0x02060C,0.55f));
        // Soft horizontal contact shadows on the lake beneath every cover.
        // No reflected image or texture readback; use existing DrawList primitives.
        const float water_y=card.y+card.h-6.0f;
        const float water_shadow_w=card.w*(1.08f-depth*0.04f);
        const float water_shadow_h=24.0f*scale;
        const float shadow_alpha=depth<0.55f?0.72f:0.42f;
        list.shadow({card.x+(card.w-water_shadow_w)*0.5f,water_y,
                     water_shadow_w,water_shadow_h},
                    22.0f*scale,14.0f*scale,Color::rgb(0x010307,shadow_alpha));
        list.shadow({card.x+card.w*0.12f,water_y+8.0f*scale,
                     card.w*0.76f,18.0f*scale},
                    16.0f*scale,10.0f*scale,Color::rgb(0x020509,shadow_alpha*0.75f));
        const auto shadow_tex=covers_.get("/app0/assets/ui/cover_shadow.png");
        if(shadow_tex)list.image(shadow_tex,{card.x-24,card.y-22,card.w+48,card.h+58},gfx::kFullUv,
            Color::rgb(0xFFFFFF,depth<0.55f?0.96f:0.59f),0);
        else if(depth<0.55f)list.shadow({card.x-4,card.y-5,w+8,h+10},28,19,Color::rgb(0x000000,0.65f));
        else list.shadow(card,13,13,Color::rgb(0x000000,0.43f));
        // Focus halo belongs to the single selected cover, not neighbouring cases.
        if(depth<0.15f)
            list.glow({card.x-6,card.y-6,card.w+12,card.h+12},26,12,
                Color::rgb(0xFAF4E8,0.22f));
        list.rounded_rect(card,18,Color::rgb(0x060B12,depth>1.2f?0.82f:0.98f));
        const float inset=with_case_header?10.0f:0.0f;
        const float header_h=with_case_header?std::max(28.0f,card.h*kFanHeaderHeightRatio):0.0f;
        const gfx::Rect art_rect=with_case_header
            ? gfx::Rect{card.x+inset,card.y+header_h+2.0f,card.w-inset*2.0f,std::max(2.0f,card.h-header_h-inset-2.0f)}
            : card;
        const auto image=covers_.get(g.cover_path);
        if(image && g.available){
            const auto size=cover_dimensions;
            // Synthetic PS4/PS5/Switch headers keep their original crop policy.
            // PS2/Xbox/Homebrew art has its own printed box edges, which must
            // remain complete: preserve aspect ratio and use full UVs (contain).
            const gfx::Rect image_rect=with_case_header?art_rect:
                cover_contain_rect(size.first,size.second,art_rect);
            const gfx::Rect image_uv=with_case_header?
                uv_cover_fit(size.first,size.second,art_rect):gfx::kFullUv;
            list.image(image,image_rect,image_uv,
                Color::rgb(0xFFFFFF,depth>1.0f?0.84f:1.0f),with_case_header?12:16);
        } else {
            list.gradient_rect(card,18,Color::rgb(0x0F2541),platform_color(g.platform).with_alpha(0.65f));
            if(with_case_header){
                list.rounded_rect({card.x+8,card.y+8,card.w-16,std::max(32.0f,header_h-8)},16,Color::rgb(0x0F1520,0.74f));
                translated_text(list,context_.fonts.semibold,shorten(g.platform,14),card.x+18,card.y+32,21,kInk);
            } else {
                translated_text(list,context_.fonts.semibold,shorten(g.platform,10),card.x+18,card.y+47,21,kInk);
            }
            translated_text(list,context_.fonts.semibold,shorten(g.title,19),card.x+15,card.y+card.h*0.60f,
                depth<1.0f?25:17,kInk);
            translated_text(list,context_.fonts.regular,"Kein Cover",card.x+16,card.y+card.h*0.72f,15,kMuted);
        }
        if(with_case_header){
            const char* header_path=fan_cover_header_asset(g.platform);
            const auto header=header_path?covers_.get(header_path):0;
            const gfx::Rect header_rect{card.x+6,card.y+6,card.w-12,std::max(34.0f,header_h+8)};
            if(header)list.image(header,header_rect,gfx::kFullUv,Color::rgb(0xFFFFFF,depth>1.0f?0.90f:1.0f),0);
            else {
                list.rounded_rect(header_rect,18,Color::rgb(0x0F1520,0.80f));
                translated_text(list,context_.fonts.semibold,g.platform,header_rect.x+18,header_rect.y+28,24,kInk);
            }
        }
        const auto frame=covers_.get("/app0/assets/ui/cover_frame.png");
        if(frame)list.image(frame,{card.x-5,card.y-5,card.w+10,card.h+10},gfx::kFullUv,
            Color::rgb(0xFFFFFF,depth<0.55f?0.50f:0.63f),0);
        else list.bordered_rect(card,17,Color::rgb(0xFFFFFF,0.0f),depth<0.55f?3.3f:1.2f,
            depth<0.55f?Color::rgb(0xF5E4C4):Color::rgb(0x7487AC,0.38f));
        if(depth<0.55f)list.bordered_rect(card,18,Color::rgb(0xFFFFFF,0),1.4f,
            Color::rgb(0xF6E7C7,0.78f*(1.0f-depth)));
    }
    void draw_cover_fan(gfx::DrawList &list) const {
        const int center=static_cast<int>(std::lround(visual_selection_));
        std::array<int,5> candidates{};
        std::size_t size=0;
        for(int i=center-2;i<=center+2;++i)
            if(i>=0 && i<static_cast<int>(indices_.size()) &&
               std::abs(static_cast<float>(i)-visual_selection_)<=2.45f)candidates[size++]=i;
        // Fixed-size stack array: no heap allocations while drawing each frame.
        for(std::size_t i=0;i<size;++i){
            for(std::size_t j=i+1;j<size;++j){
                const float da=std::abs(static_cast<float>(candidates[i])-visual_selection_);
                const float db=std::abs(static_cast<float>(candidates[j])-visual_selection_);
                if(db>da)std::swap(candidates[i],candidates[j]);
            }
        }
        for(std::size_t i=0;i<size;++i)draw_fan_cover(list,candidates[i]);
    }
    void draw_game_info(gfx::DrawList &list) const {
        const auto &g=catalogue_.games[indices_[static_cast<std::size_t>(selected_)]];
        const auto &fonts=context_.fonts;
        // Free-standing reference-style title (no fixed dark information tile).
        // Subtle lettering shadow preserves contrast on the bright lake background.
        const std::string title=shorten(g.title,46);
        serif_text(list,title,758,668,43,Color::rgb(0x07111C,0.78f),1090);
        serif_text(list,title,756.2f,665.8f,43,kInk,1090);
        serif_text(list,title,755,664,43,kInk,1090);
        // Full platform name as free-standing text, never clipped by a capsule width.
        const std::string platform_text=display_platform_name(g.platform);
        serif_text(list,platform_text,755,698,20,Color::rgb(0xD8E0E9));
        // Only real PS5/PS4 Title-IDs identify an individual game in the
        // ShadowMount+ log. An emulator PID is NOT a ROM's playtime.
        const std::string native_time=native_log_time_label(g);
        if(!native_time.empty()){
            const float time_x=755.0f+serif_text_width(platform_text,20.0f)+42.0f;
            serif_text(list,native_time,time_x,698,24,Color::rgb(0xBDF2DF),
                       std::max(0.0f,1710.0f-time_x));
        }
        // Real PlayStation Cross glyph and text form one measured, centred row.
        // Use the UI-kit measurement function, not a guessed fixed X offset.
        const Rect launch_button{755,718,182,49};
        list.glow({launch_button.x-6,launch_button.y-6,launch_button.w+12,launch_button.h+12},
            20,12,Color::rgb(0xF4D8AC,0.25f));
        list.gradient_rect(launch_button,18,Color::rgb(0x718097,0.58f),
            Color::rgb(0x24364F,0.70f));
        list.rounded_rect({launch_button.x+14,launch_button.y+3,launch_button.w-28,3},2,
            Color::rgb(0xFFF0D7,0.29f));
        list.bordered_rect(launch_button,18,Color::rgb(0xFFFFFF,0),1.5f,
            Color::rgb(0xF4E7D3,0.81f));
        const bool menu_mode=emulator_platform_for_title(g.title_id)!=nullptr ||
            (!emulator_boot_settings_.direct_boot &&
             (g.platform=="PS2" || g.platform=="Switch" || g.platform=="Xbox 360"));
        const ui::Hint start_hint[]={{ui::Button::cross,hint(menu_mode?"Menue":"Spielen")}};
        ui::HintLayout start_layout;
        start_layout.cy=launch_button.y+launch_button.h*0.5f;
        start_layout.size=24;
        start_layout.text_size=20;
        start_layout.item_gap=10;
        const float start_width=ui::measure_hints(fonts,start_hint,1,start_layout);
        const float start_x=launch_button.x+(launch_button.w-start_width)*0.5f;
        ui::draw_hints(list,fonts,ui::GlyphStyle::dark(),start_hint,1,start_x,false,start_layout);
        if(!launch_message_.empty())serif_text(list,shorten(launch_message_,93),755,802,18,
            launch_in_progress_?kMuted:Color::rgb(0xFFE0B0),1080);
        if(favorites_.count(g.id)>0)list.star(1177,652,13,Color::rgb(0xFFDCA1));
    }
    std::array<int,3> recent_indices() const {
        std::array<int,3> out{{-1,-1,-1}};
        unsigned count=0;
        for(const auto &token:recent_list_){
            for(std::size_t i=0;i<catalogue_.games.size();++i){
                if(!recent_matches(catalogue_.games[i],token))continue;
                bool already_added=false;
                for(unsigned j=0;j<count;++j)if(out[j]==static_cast<int>(i))already_added=true;
                if(!already_added)out[count++]=static_cast<int>(i);
                break;
            }
            if(count==3)break;
        }
        return out;
    }
    unsigned recent_card_count() const {
        const auto entries=recent_indices();
        unsigned count=0;
        for(const int index:entries)if(index>=0)++count;
        return count;
    }
    void glass_card(gfx::DrawList &list,Rect r,float radius) const {
        if(theme_settings_.frosted_glass){
            list.shadow({r.x,r.y+10,r.w,r.h},radius+6,22,Color::rgb(0x01050A,0.48f));
            // Screen-aligned blurred lake texture, precomputed offline. No
            // real-time shader/filter/readback or asset decoding in draw().
            const auto blurred=covers_.get("/app0/assets/ui/frosted_lake_backdrop.png");
            if(blurred){
                const gfx::Rect lake_uv{r.x/1920.0f,r.y/1080.0f,r.w/1920.0f,r.h/1080.0f};
                list.image(blurred,r,lake_uv,Color::rgb(0xFFFFFF,0.93f),radius);
            }
            list.gradient_rect(r,radius,Color::rgb(0x122A34,0.72f),Color::rgb(0x0A1822,0.82f));
            list.bordered_rect(r,radius,Color::rgb(0xFFFFFF,0),1.2f,Color::rgb(0x76B7BE,0.32f));
            list.rounded_rect({r.x+radius,r.y+2,r.w-radius*2,1},1,Color::rgb(0xEAF8FA,0.35f));
        } else {
            list.shadow({r.x,r.y+7,r.w,r.h},radius,15,Color::rgb(0x000000,0.24f));
            list.gradient_rect(r,radius,Color::rgb(0x0D1F2A,0.36f),Color::rgb(0x08151E,0.46f));
            list.bordered_rect(r,radius,Color::rgb(0xFFFFFF,0),1.0f,Color::rgb(0xA8D0D6,0.22f));
            list.rounded_rect({r.x+radius,r.y+1,r.w-radius*2,1},1,Color::rgb(0xEAF8FA,0.25f));
        }
    }
    void draw_recent_cards(gfx::DrawList &list) const {
        serif_text(list,"Zuletzt gespielt",754,847,22,kInk);
        serif_text(list,"Startaufträge – kein Nachweis der Spiel-Laufzeit",960,845,14,kMuted,700);
        const auto recent=recent_indices();
        if(recent_save_error_)serif_text(list,"Verlauf konnte nicht gespeichert werden",1040,846,17,Color::rgb(0xFFD3A3));
        for(int i=0;i<3;++i){
            const float x=744+i*379.0f;
            const Rect card{x,872,363,108};
            glass_card(list,card,18);
            if(recent_focus_ && recent_selected_==i)
                list.bordered_rect(card,18,Color::rgb(0xFFFFFF,0),2,Color::rgb(0xF0DFC0,0.70f));
            if(recent[i]<0){
                serif_text(list,"Deine gestarteten Spiele erscheinen hier",x+18,933,16,kMuted,328);
                continue;
            }
            const auto &g=catalogue_.games[static_cast<std::size_t>(recent[i])];
            const auto cover=covers_.get(g.cover_path);
            if(cover)list.image(cover,{x+14,886,78,78},gfx::kFullUv,Color::rgb(0xFFFFFF),11);
            else list.rounded_rect({x+14,886,78,78},11,platform_color(g.platform).with_alpha(0.24f));
            serif_text(list,shorten(g.title,27),x+106,922,19,kInk,244);
            serif_text(list,display_platform_name(g.platform)+" · Startauftrag",x+106,950,16,kMuted,240);
        }
    }
    void draw_controllers(gfx::DrawList &list) const {
        // The same four positions as before; never mark an unverified pad active.
        for(unsigned i=0;i<4;++i){
            const float x=1298+i*82.0f,y=26;
            const int status=controllers_.state[i];
            const bool connected=(status==1);
            const auto color=connected?Color::rgb(0xFDFBF7,0.98f):
                Color::rgb(0x6A7588,0.38f);
            if(connected)
                list.glow({x-5,y-1,48,43},15,8,Color::rgb(0xF5E2BC,0.18f));
            const auto pad=covers_.get("/app0/assets/ui/controller.png");
            if(pad)list.image(pad,{x-4,y,42,42},gfx::kFullUv,color,0);
            else {
                list.rounded_rect({x,y+4,45,21},10,color);
                list.rounded_rect({x,y+15,12,13},6,color);
                list.rounded_rect({x+33,y+15,12,13},6,color);
                const auto dark=Color::rgb(0x1A283C);
                list.rounded_rect({x+9,y+9,3,10},1,dark);
                list.rounded_rect({x+5,y+13,11,3},1,dark);
                list.rounded_rect({x+31,y+10,4,4},2,dark);
                list.rounded_rect({x+36,y+15,4,4},2,dark);
            }
            translated_text(list,context_.fonts.regular,
                std::to_string(i+1)+(status<0?" ?":""),x+43,y+26,14,color);
            if(connected){
                // Small warm status LED, not a second neon border.
                list.rounded_rect({x+43,y+35,8,3},1.5f,Color::rgb(0xE8D19F,0.85f));
                // Battery information is absent from our verified ScePad sample.
                // Draw a gauge only after an actually valid battery reading exists.
                const int battery=controllers_.battery[i];
                if(battery>=0 && battery<=100){
                    list.bordered_rect({x,y+46,35,10},2,Color::rgb(0xFFFFFF,0),1.0f,
                        Color::rgb(0xEEE3D2,0.70f));
                    list.rounded_rect({x+35,y+49,3,4},1,Color::rgb(0xEEE3D2,0.65f));
                    const float inner_width=31.0f*static_cast<float>(battery)/100.0f;
                    if(inner_width>0)
                        list.rounded_rect({x+2,y+48,inner_width,6},1,
                            Color::rgb(0xE8D19F,0.85f));
                    translated_text(list,context_.fonts.regular,
                        std::to_string(battery)+"%",x+43,y+56,13,color);
                    if(controllers_.charging[i]==1)
                        translated_text(list,context_.fonts.semibold,"+",x+32,y+44,13,
                            Color::rgb(0xE8D19F));
                }
            }
        }
    }
    // Hardware silhouettes are bundled native artwork, not font placeholders.
    void draw_platform_icon(gfx::DrawList &list,int platform,float x,float y,Color tint) const {
        static constexpr const char* names[]={"","ps5","ps4","ps2","switch","xbox360"};
        if(platform>=1 && platform<=5){
            const auto texture=covers_.get(std::string("/app0/assets/platforms/")+names[platform]+".png");
            if(texture){list.image(texture,{x-5,y-6,35,38},gfx::kFullUv,Color::rgb(0xFFFFFF),0);return;}
        }
        if(platform==7){
            const auto texture=covers_.get("/app0/assets/ui/star.png");
            if(texture){list.image(texture,{x-5,y-7,37,37},gfx::kFullUv,Color::rgb(0xFFFFFF),0);return;}
            list.star(x+11,y+13,10,tint);return;
        }
        if(platform==8){
            const auto texture=covers_.get("/app0/assets/ui/clock.png");
            if(texture){list.image(texture,{x-5,y-7,37,37},gfx::kFullUv,Color::rgb(0xFFFFFF),0);return;}
            list.bordered_rect({x+1,y+2,23,23},12,Color::rgb(0xFFFFFF,0.0f),2,tint);
            list.rounded_rect({x+12,y+7,2,8},1,tint);
            list.rounded_rect({x+12,y+14,6,2},1,tint);return;
        }
        if(platform==4){
            list.rounded_rect({x,y+2,11,25},5,tint);
            list.rounded_rect({x+13,y+2,11,25},5,tint);
            list.rounded_rect({x+4,y+8,3,3},2,Color::rgb(0x0D1C32));
            list.rounded_rect({x+17,y+18,3,3},2,Color::rgb(0x0D1C32));return;
        }
        if(platform==5){
            list.rounded_rect({x,y+2,24,24},12,tint);
            list.rounded_rect({x+10,y+4,3,20},1,Color::rgb(0x133763));
            list.rounded_rect({x+3,y+12,18,3},1,Color::rgb(0x133763));return;
        }
        if(platform==0 || platform==6){
            const auto texture=covers_.get(platform==0?"/app0/assets/ui/grid.png":"/app0/assets/ui/emulator.png");
            if(texture){list.image(texture,{x-5,y-7,37,37},gfx::kFullUv,Color::rgb(0xFFFFFF),0);return;}
            list.rounded_rect({x+1,y+3,10,10},3,tint);
            list.rounded_rect({x+14,y+3,10,10},3,tint);
            list.rounded_rect({x+1,y+16,10,10},3,tint);
            list.rounded_rect({x+14,y+16,10,10},3,tint);return;
        }
        list.rounded_rect({x+3,y+2,18,25},5,tint);
        list.rounded_rect({x+7,y+6,10,14},2,Color::rgb(0x0F2340));
        list.rounded_rect({x+9,y+23,6,2},1,Color::rgb(0x0F2340));
    }
    void draw_platform_bar(gfx::DrawList &list) const {
        // Navigation/footer remain fixed; only the platform strip scrolls.
        const int platform_count=std::max(0,static_cast<int>(sidebar_items_.size())-4);
        constexpr int visible=5;
        int active_platform=tab_-1;
        int first=0;
        if(active_platform>=0 && active_platform<platform_count){first=std::clamp(active_platform-visible+1,0,
                                             std::max(0,platform_count-visible));}
        const auto item_at=[&](int index,float y){
            if(index<0 || index>=static_cast<int>(sidebar_items_.size()))return;
            const auto &entry=sidebar_items_[static_cast<std::size_t>(index)];
            const bool selected=tab_==index;
            if(selected){
                list.glow({38,y-29,208,60},15,10,Color::rgb(0x3B887B,0.22f));
                list.rounded_rect({38,y-29,208,60},13,Color::rgb(0x173E3E,0.38f));
                list.bordered_rect({38,y-29,208,60},13,Color::rgb(0xFFFFFF,0),1.5f,
                    sidebar_focus_?Color::rgb(0x7CE0C8,0.70f):Color::rgb(0x56B3A0,0.45f));
            }
            // New platforms may have icons later; never invent image paths.
            if(entry.is_platform && !entry.icon.empty()){
                const auto path=std::string("/app0/assets/platforms/")+entry.icon;
                const auto texture=covers_.get(path);
                if(texture)list.image(texture,{46,y-19,35,38},gfx::kFullUv,Color::rgb(0xFFFFFF),0);
                else draw_platform_icon(list,entry.legacy_icon,51,y-13,Color::rgb(0xEEE4D6));
            }else draw_platform_icon(list,entry.legacy_icon,51,y-13,Color::rgb(0xEEE4D6));
            serif_text(list,entry.label,92,y+7,24,selected?kInk:Color::rgb(0xEEE4D7),154);
        };
        item_at(0,165);
        for(int i=first;i<platform_count && i<first+visible;++i){
            item_at(i+1,250.0f+static_cast<float>(i-first)*85.0f);
        }
        if(platform_count>visible){
            list.rounded_rect({255,220,2,393},1,Color::rgb(0xC5D2DF,0.20f));
            const float thumb=std::max(35.0f,393.0f*static_cast<float>(visible)/platform_count);
            const float y=220.0f+(393.0f-thumb)*static_cast<float>(first)/
                          std::max(1,platform_count-visible);
            list.rounded_rect({254,y,4,thumb},2,Color::rgb(0xA7E7D9,0.80f));
        }
        list.rounded_rect({46,645,200,2},1,Color::rgb(0xE6D6B4,0.32f));
        const int n=static_cast<int>(sidebar_items_.size());
        item_at(n-3,703);item_at(n-2,788);item_at(n-1,871);
    }
    void draw_detail(app::Frame &frame) const {
        auto &list=frame.overlay;const auto &fonts=context_.fonts;
        if(indices_.empty())return;
        const auto &g=catalogue_.games[indices_[static_cast<std::size_t>(selected_)]];
        frame.glass=true;
        list.rounded_rect({265,125,1390,790},36,Color::rgb(0x151D2D,0.97f));
        list.rounded_rect({315,186,350,525},28,Color::rgb(0x26314C));
        list.rounded_rect({345,215,137,50},16,platform_color(g.platform));
        translated_text(list,fonts.semibold,g.platform,359,251,29,Color::rgb(0xFFFFFF));
        const auto cover=covers_.get(g.cover_path);
        if(cover && g.available)list.image(cover,{320,192,340,513},gfx::kFullUv,Color::rgb(0xFFFFFF),22);
        else{
            translated_text(list,fonts.display,"COVER",354,516,57,kMuted);
            translated_text(list,fonts.regular,"Kein lokales Cover gefunden",344,653,21,kMuted);
        }
        translated_text(list,fonts.display,shorten(g.title,35),710,244,43,kInk);
        translated_text(list,fonts.semibold,"Plattform: "+g.platform,710,309,27,kAccent);
        const std::string native_detail_time=native_log_time_label(g);
        if(!native_detail_time.empty())
            translated_text(list,fonts.semibold,native_detail_time,1060,309,28,kAccent);
        else if(g.platform=="PS2" || g.platform=="Switch" || g.platform=="Xbox 360")
            translated_text(list,fonts.regular,"ROM-Spielzeit noch nicht messbar",1060,309,21,kMuted);
        translated_text(list,fonts.regular,"Format: "+g.format,710,353,23,kMuted);
        translated_text(list,fonts.regular,"Spielpfad:",710,401,23,kMuted);
        translated_text(list,fonts.regular,shorten(g.path,78),710,436,20,kInk);
        if(g.platform=="PS4")
            translated_text(list,fonts.semibold,"Title-ID: "+(g.title_id.empty()?std::string("FEHLT"):g.title_id),710,466,19,kAccent);
        const auto plan=ulg::make_launch_plan(g.platform,g.path);
        const bool ps5_container=(g.platform=="PS5" && (g.format=="ffpfsc" || g.format=="exfat"));
        const bool ps5_direct=(g.platform=="PS5" && ulg::make_registered_ps5_launch_plan(g.title_id).supported);
        const bool ps4_direct=(g.platform=="PS4" && ulg::make_registered_ps4_launch_plan(g.title_id).supported);
        const bool ps2_menu=(g.platform=="PS2" && ulg::make_ps5sx2_menu_launch_plan().supported);
        const bool can_start=g.available && (plan.supported || ps5_direct || ps4_direct || ps2_menu);
        list.rounded_rect({711,506,800,125},20,can_start?Color::rgb(0x3D527C,0.70f):Color::rgb(0x2C3044,0.70f));
        if(can_start){
            const bool menu_mode=!emulator_boot_settings_.direct_boot &&
                (g.platform=="PS2" || g.platform=="Switch" || g.platform=="Xbox 360");
            const ui::Hint detail_start[]={{ui::Button::cross,hint(menu_mode?"Emulator-Menue":"Spielen")}};
            ui::HintLayout detail_layout;
            detail_layout.cy=557;detail_layout.size=28;detail_layout.text_size=27;detail_layout.item_gap=16;
            ui::draw_hints(list,fonts,ui::GlyphStyle::dark(),detail_start,1,742,false,detail_layout);
        } else translated_text(list,fonts.semibold,
            g.available?"DIREKTSTART NICHT VERFUEGBAR":"SPIEL NICHT VERFUEGBAR",740,559,27,kMuted);
        const bool only_menu=!emulator_boot_settings_.direct_boot &&
            (g.platform=="PS2" || g.platform=="Switch" || g.platform=="Xbox 360");
        translated_text(list,fonts.regular,can_start?(only_menu?"Startet Emulator-Menue ohne ROM-Startauftrag":(ps5_direct?(ps5_container?"Container-Teststart via PPSA-ID/Launcher-ELF 10199":"Systemstart ueber den vorhandenen Forwarder"):
            (ps4_direct?"PS4: aktiver Forwarder auf Port 10199 erforderlich":
            (ps2_menu?"PS2-Spielpfad via disc-launch.txt; Quadrat: Menue":"Vorhandener Startdienst erforderlich")))):(g.available?(ps5_container?"Container: gueltige PPSA-ID und Launcher-ELF erforderlich":plan.reason):"Datentraeger erneut anschliessen"),740,599,20,Color::rgb(0xFFB982));
        if(!launch_message_.empty())translated_text(list,fonts.regular,shorten(launch_message_,90),714,
            (g.platform=="PS5" && local_services_checked_)?659:684,22,kAccent);
        const bool ps5_network_diag=((g.platform=="PS5" || g.platform=="PS4") && local_services_checked_);
        if(ps5_network_diag){
            // Readable, separate results instead of the previous binary combined
            // status. A listener is NOT proof that the expected daemon answers.
            const auto format_port=[](const char *name,const ulg::LocalPortProbe &p){
                std::string line=std::string("TCP ")+name+": ";
                if(p.reachable)return line+"ERREICHBAR (nur TCP)";
                return line+"errno="+std::to_string(p.os_error)+" "+
                    ulg::probe_errno_name(p.os_error)+" ["+
                    ulg::probe_stage_name(p.stage)+"]";
            };
            translated_text(list,fonts.regular,format_port("10199",local_services_.launcher),714,684,19,kAccent);
            translated_text(list,fonts.regular,format_port("9021",local_services_.elfldr),714,707,19,kAccent);
            translated_text(list,fonts.regular,format_port("2121 FTP",local_services_.ftp),714,730,19,kAccent);
            translated_text(list,fonts.regular,"Nur Verbindungsprobe; kein Payload gesendet",714,752,16,kMuted);
        }
        if(g.platform=="PS5" || g.platform=="PS4" || g.platform=="PS2" ||
           g.platform=="Switch" || g.platform=="Xbox 360"){
            const auto it=cover_diagnostics_.find(g.id);
            const std::string status=it!=cover_diagnostics_.end()?it->second:
                (g.cover_path.empty()?"KEIN COVERPFAD (POSIX/FTP/SCAN)":"BILDPFAD VORHANDEN; LADEVERSUCH AUSSTEHEND");
            translated_text(list,fonts.regular,"COVER-DIAGNOSE: "+shorten(status,ps5_network_diag?53:65),720,
                ps5_network_diag?813:777,19,kAccent);
            translated_text(list,fonts.regular,"Bilder kommen nur aus lokalen Emulator-Caches",720,
                ps5_network_diag?838:805,17,kMuted);
        }
        translated_text(list,fonts.regular,"Dreieck: Favorit   |   Kreis: Zurueck",720,
            ps5_network_diag?784:736,23,kMuted);
        if(g.platform=="PS5" || g.platform=="PS4")
            translated_text(list,fonts.semibold,"Quadrat: TCP 10199 / 9021 / 2121 (Diagnose)",720,864,21,kInk);
        if(g.platform=="PS2")
            translated_text(list,fonts.semibold,"Quadrat: PS5SX2-Menue (Forwarder ohne Autostart)",720,864,22,kInk);
        if(g.platform=="Switch" || g.platform=="Xbox 360")
            translated_text(list,fonts.semibold,"Quadrat: Emulator oeffnen (Test)",720,864,22,kInk);
    }
    // V1.1.8: bounded read-only experiment; invoked on launch and when user
    // opens details. Missing/incomplete logs remain "unknown", never 0 hours.
    void refresh_playtime_log(){
        std::string bytes;
        bool ok=ulg::playtime::read_local_log("/data/shadowmount/debug.log",bytes);
        if(!ok){
            // The fakeLib mount may not expose /data to this sandbox. Try the
            // existing READ-ONLY loopback ftpsrv, never any mutating FTP verb.
            ulg::FtpReadonly ftp;
            if(ftp.connect_local())ok=ftp.retrieve(
                "/data/shadowmount/debug.log",bytes,ulg::playtime::kMaxLogBytes);
        }
        if(ok)playtime_log_=ulg::playtime::analyze(bytes);
        else playtime_log_={};
    }
    std::string native_log_time_label(const ulg::Game &g) const {
        // Fail closed for emulator ROMs: ShadowMount+ only exposes the
        // emulator's PPSA process, not the ROM running inside it. Never use
        // PPSA99203 / PPSA99008 / PPSA50011 as a game's playtime.
        if(!ulg::playtime::is_native_game_id(g.platform,g.title_id))return {};
        if(!playtime_log_.readable)return {};
        const auto it=playtime_log_.by_title.find(g.title_id);
        if(it==playtime_log_.by_title.end() || it->second.sessions==0)return {};
        return "Spielzeit: "+ulg::playtime::duration_label(it->second.seconds);
    }
    void set_message(std::string message){settings_message_=std::move(message);}
    void save_roots_and_rescan(){
        if(!ulg::save_user_roots(roots_,kUserPathsFile)){
            set_message("SPEICHERFEHLER: Persoenliche Pfade konnten nicht gespeichert werden.");
            return;
        }
        refresh();set_message("Gespeichert und erneut gescannt.");
    }
    void change_add_platform(int direction){
        const int count=static_cast<int>(kPlatforms.size());
        add_platform_=(add_platform_+count+direction)%count;
    }
    // View filter is presentation-only. It never changes the user's configured
    // scan paths or platform assignment. L1/R1 selects All/PS5/PS4/... .
    std::vector<std::size_t> settings_visible_roots() const {
        std::vector<std::size_t> out;
        for(std::size_t i=0;i<roots_.size();++i)
            if(settings_filter_==0 ||
               roots_[i].platform==kPlatforms[static_cast<std::size_t>(settings_filter_-1)])
                out.push_back(i);
        return out;
    }
    bool save_emulator_boot_setting(bool direct_boot){
        if(emulator_boot_settings_.revision==UINT32_MAX){
            emulator_settings_message_="Einstellung kann nicht mehr versioniert werden";
            return false;
        }
        ulg::EmulatorBootSettings next=emulator_boot_settings_;
        next.direct_boot=direct_boot;
        ++next.revision;
        // A successful write to at least one private mount is required before
        // the visible setting changes. The revision resolves partial writes.
        const bool primary=ulg::write_emulator_boot_settings(next,kPrimaryEmulatorSettingsFile);
        const bool fallback=ulg::write_emulator_boot_settings(next,kFallbackEmulatorSettingsFile);
        if(!primary && !fallback){
            emulator_settings_message_="SPEICHERFEHLER: Startmodus bleibt unveraendert";
            return false;
        }
        emulator_boot_settings_=next;
        emulator_settings_message_=(primary && fallback)?"Startmodus dauerhaft gespeichert":
            "Startmodus gespeichert (ein Speicherort erreichbar)";
        return true;
    }
    bool save_language_setting(bool english){
        if(language_settings_.revision==UINT32_MAX){
            language_settings_message_="Language setting revision exhausted";
            return false;
        }
        ulg::LanguageSettings next=language_settings_;
        next.english=english;
        ++next.revision;
        const bool primary=ulg::write_language_settings(next,kPrimaryLanguageFile);
        const bool fallback=ulg::write_language_settings(next,kFallbackLanguageFile);
        if(!primary && !fallback){
            language_settings_message_="Could not save language / Sprache nicht gespeichert";
            return false;
        }
        language_settings_=next;
        language_settings_message_=english?"Language saved: English":"Sprache gespeichert: Deutsch";
        return true;
    }
    bool save_theme_setting(bool frosted){
        if(theme_settings_.revision==UINT32_MAX){
            theme_settings_message_="Design-Revision erschoepft";
            return false;
        }
        ulg::ThemeSettings next=theme_settings_;
        next.frosted_glass=frosted;
        ++next.revision;
        const bool primary=ulg::write_theme_settings(next,kPrimaryThemeFile);
        const bool fallback=ulg::write_theme_settings(next,kFallbackThemeFile);
        if(!primary && !fallback){
            theme_settings_message_="Kachel-Design konnte nicht gespeichert werden";
            return false;
        }
        theme_settings_=next;
        theme_settings_message_=frosted?"Frosted Petrol gespeichert":"Transparent gespeichert";
        return true;
    }
    bool save_music_setting(bool enabled){
        if(music_settings_.revision==UINT32_MAX){
            music_settings_message_="Musik-Revision erschoepft";return false;
        }
        ulg::MusicSettings next=music_settings_;
        next.enabled=enabled;
        ++next.revision;
        const bool primary=ulg::write_music_settings(next,kPrimaryMusicFile);
        const bool fallback=ulg::write_music_settings(next,kFallbackMusicFile);
        if(!primary && !fallback){
            music_settings_message_="Musikeinstellung konnte nicht gespeichert werden";
            return false;
        }
        music_settings_=next;
        music_settings_message_=enabled?"Musik AN gespeichert":"Musik AUS gespeichert";
        return true;
    }
    template<class... Args>
    void translated_text(gfx::DrawList &list,const ui::FontRef &face,
                         const std::string &value,Args&&... args) const {
        ui::text(list,face,ulg::translate_ui(value,language_settings_.english),
                 std::forward<Args>(args)...);
    }
    const char *hint(const char *label) const {
        return ulg::tr_hint(label,language_settings_.english);
    }
    void update_settings(const InputFrame &input,app::Feedback &feedback){
        // L1/R1 switches Settings tabs only (the main library keeps its own behavior).
        if(input.is_pressed(Action::page_prev)||input.is_pressed(Action::page_next)){
            const int step=input.is_pressed(Action::page_next)?1:-1;
            const int count=static_cast<int>(kSettingsTabs.size());
            settings_tab_=(settings_tab_+count+step)%count;
            feedback.play(audio::Cue::tab);
            return;
        }
        // Appearance: three independent settings, using Up/Down to move and X
        // to toggle the selected option. No launch or library state changes.
        if(settings_tab_==0){update_emulator_settings(input,feedback);return;}
        if(settings_tab_==1){
            if(input.nav==Direction::up){
                presentation_selected_=std::max(0,presentation_selected_-1);
                feedback.play(audio::Cue::focus);return;
            }
            if(input.nav==Direction::down){
                presentation_selected_=std::min(3,presentation_selected_+1);
                feedback.play(audio::Cue::focus);return;
            }
            if(input.is_pressed(Action::confirm)){
                bool changed=false;
                if(presentation_selected_==0)changed=save_language_setting(!language_settings_.english);
                else if(presentation_selected_==1)
                    changed=save_emulator_boot_setting(!emulator_boot_settings_.direct_boot);
                else if(presentation_selected_==2)
                    changed=save_theme_setting(!theme_settings_.frosted_glass);
                else if(presentation_selected_==3)
                    changed=save_music_setting(!music_settings_.enabled);
                feedback.play(changed?audio::Cue::select:audio::Cue::error);
            }
            return;
        }
        if(settings_tab_!=3)return; // Legacy folder tab retired; preserve its helpers for compatibility.
        // Inside the retired folder tab, L2/R2 preserved the old platform filter.
        if(input.is_pressed(Action::jump_prev)||input.is_pressed(Action::jump_next)){
            const int count=static_cast<int>(kPlatforms.size()+1);
            const int step=input.is_pressed(Action::jump_next)?1:-1;
            settings_filter_=(settings_filter_+count+step)%count;
            const auto filtered=settings_visible_roots();
            settings_selected_=filtered.empty()?-1:static_cast<int>(filtered.front());
            if(settings_filter_>0)add_platform_=settings_filter_-1;
            feedback.play(audio::Cue::focus);
        }
        const auto visible=settings_visible_roots();
        if(!visible.empty()){
            const auto at=std::find(visible.begin(),visible.end(),static_cast<std::size_t>(settings_selected_));
            int cursor=at==visible.end()?0:static_cast<int>(at-visible.begin());
            if(input.nav==Direction::up)cursor=std::max(0,cursor-1);
            if(input.nav==Direction::down)cursor=std::min(static_cast<int>(visible.size())-1,cursor+1);
            settings_selected_=static_cast<int>(visible[static_cast<std::size_t>(cursor)]);
        }
        if(input.is_pressed(Action::west)){refresh();set_message("Nur AKTIVE Ordner erneut gescannt.");feedback.play(audio::Cue::select);}
        if(input.is_pressed(Action::confirm) && settings_selected_>=0 &&
           static_cast<std::size_t>(settings_selected_)<roots_.size()){
            auto &root=roots_[static_cast<std::size_t>(settings_selected_)];
            root.enabled=!root.enabled;
            save_roots_and_rescan();feedback.play(audio::Cue::select);
        }
        if(input.is_pressed(Action::north)){
            if(settings_selected_>=0 && static_cast<std::size_t>(settings_selected_)<roots_.size()){
                for(std::size_t i=0;i<kPlatforms.size();++i)
                    if(roots_[static_cast<std::size_t>(settings_selected_)].platform==kPlatforms[i])
                        add_platform_=static_cast<int>(i);
            }
            emulator_path_edit_=false;
            browser_open_=true;browser_path_="/";browser_selected_=0;refresh_browser();
            feedback.play(audio::Cue::open);
        }
        if(input.is_pressed(Action::touch)){
            emulator_path_edit_=false;
            if(settings_selected_>=0 && static_cast<std::size_t>(settings_selected_)<roots_.size()){
                edited_root_=settings_selected_;
                input_path_=roots_[static_cast<std::size_t>(settings_selected_)].path;
                for(std::size_t i=0;i<kPlatforms.size();++i)
                    if(roots_[static_cast<std::size_t>(settings_selected_)].platform==kPlatforms[i])
                        add_platform_=static_cast<int>(i);
            }else {edited_root_=-1;input_path_="/";}
            path_key_=0;path_editor_open_=true;feedback.play(audio::Cue::open);
        }
    }
    void refresh_browser(){browser_=ulg::browse_folders(browser_path_);browser_selected_=0;}
    void add_root_path(const std::string &path){
        ulg::Root candidate{kPlatforms[static_cast<std::size_t>(add_platform_)],path};
        if(!ulg::valid_root(candidate)){set_message("Ungueltiger Pfad (absolute Ordner ohne .. erforderlich).");return;}
        for(std::size_t i=0;i<roots_.size();++i){
            if(static_cast<int>(i)==edited_root_)continue;
            if(roots_[i].path==candidate.path && roots_[i].platform==candidate.platform){
                set_message("Dieser Ordner ist fuer die Plattform bereits vorhanden.");return;
            }
        }
        if(edited_root_>=0 && static_cast<std::size_t>(edited_root_)<roots_.size()){
            candidate.enabled=roots_[static_cast<std::size_t>(edited_root_)].enabled;
            roots_[static_cast<std::size_t>(edited_root_)]=std::move(candidate);
        }else if(roots_.size()<128){
            roots_.push_back(std::move(candidate));settings_selected_=static_cast<int>(roots_.size()-1);
        }else {set_message("Maximal 128 Ordner moeglich.");return;}
        save_roots_and_rescan();browser_open_=false;path_editor_open_=false;
    }
    void update_browser(const InputFrame &input,app::Feedback &feedback){
        if(!emulator_path_edit_ && input.is_pressed(Action::page_prev))change_add_platform(-1);
        if(!emulator_path_edit_ && input.is_pressed(Action::page_next))change_add_platform(1);
        const int count=static_cast<int>(browser_.entries.size());
        if(input.nav==Direction::up && count>0)browser_selected_=std::max(0,browser_selected_-1);
        if(input.nav==Direction::down && count>0)browser_selected_=std::min(count-1,browser_selected_+1);
        if(input.is_pressed(Action::confirm) && count>0){
            browser_path_=browser_.entries[static_cast<std::size_t>(browser_selected_)].path;
            refresh_browser();feedback.play(audio::Cue::select);
        }
        if(input.is_pressed(Action::west)){
            // Only the directory currently displayed is selected, never a game file.
            edited_root_=-1;
            if(emulator_path_edit_)save_emulator_path(browser_path_);
            else add_root_path(browser_path_);
            feedback.play(audio::Cue::select);
        }
        if(input.is_pressed(Action::touch)){
            const auto end=browser_path_.find_last_of('/');
            browser_path_=end==0?"/":browser_path_.substr(0,end);
            refresh_browser();feedback.play(audio::Cue::focus);
        }
        if(input.is_pressed(Action::north)){
            edited_root_=-1;input_path_=browser_path_=="/"?"/":browser_path_+"/";
            path_key_=0;path_editor_open_=true;feedback.play(audio::Cue::open);
        }
    }
    void update_path_editor(const InputFrame &input,app::Feedback &feedback){
        constexpr int columns=9;
        const int count=static_cast<int>(std::strlen(kPathKeys));
        int next=path_key_;
        if(input.nav==Direction::left)next--;
        if(input.nav==Direction::right)next++;
        if(input.nav==Direction::up)next-=columns;
        if(input.nav==Direction::down)next+=columns;
        next=std::clamp(next,0,count-1);
        if(next!=path_key_){path_key_=next;feedback.play(audio::Cue::focus);}
        if(input.is_pressed(Action::west) && input_path_.size()>1)input_path_.pop_back();
        if(input.is_pressed(Action::north))path_uppercase_=!path_uppercase_;
        if(input.is_pressed(Action::confirm)){
            char key=kPathKeys[path_key_];
            if(key=='<'){if(input_path_.size()>1)input_path_.pop_back();}
            else if(key=='+'){if(emulator_path_edit_)save_emulator_path(input_path_);else add_root_path(input_path_);}
            else if(input_path_.size()<240){
                if(key>='A'&&key<='Z'&&!path_uppercase_)key=static_cast<char>(key-'A'+'a');
                input_path_+=key;
            }
            feedback.play(audio::Cue::select);
        }
    }
    std::string state_label(const ulg::RootState &root) const {
        if(root.status=="ok")return "OK";
        if(root.status=="ftp_ok")return "FTP lesbar";
        if(root.status=="deaktiviert")return "AUS";
        if(root.status=="usb_offline")return "USB unsichtbar";
        if(root.status=="offline")return "nicht sichtbar";
        if(root.status=="zugriff_verweigert")return "Zugriff verweigert";
        if(root.status=="kein_ordner")return "kein Ordner";
        return "Fehler";
    }
    // The four-tab Settings shell retains the existing game/library state,
    // path storage and history persistence still belong to their old owners.
    void draw_settings(app::Frame &frame) const {
        frame.glass=true;
        auto &list=frame.overlay;
        const auto &fonts=context_.fonts;
        list.rounded_rect({128,62,1664,963},30,Color::rgb(0x151D2D,0.98f));
        translated_text(list,fonts.display,"Einstellungen",185,137,42,kInk);
        translated_text(list,fonts.semibold,kBuildLabel,1230,133,15,kAccent);
        // Tab buttons visually echo the shoulder buttons; only one is active.
        for(std::size_t i=0;i<kSettingsTabs.size();++i){
            const float x=184+static_cast<float>(i)*520.0f;
            const Rect tab_rect{x,171,514,62};
            const bool active=settings_tab_==static_cast<int>(i);
            list.rounded_rect(tab_rect,15,active?Color::rgb(0x536B8B,0.93f):Color::rgb(0x263448,0.74f));
            list.bordered_rect(tab_rect,15,Color::rgb(0xFFFFFF,0),1.2f,
                active?Color::rgb(0xE5D5B6,0.82f):Color::rgb(0xC5D2E1,0.20f));
            translated_text(list,fonts.semibold,kSettingsTabs[i],x+17,210,21,
                active?kInk:Color::rgb(0xBBC5D1,0.76f));
        }
        if(settings_tab_==0)draw_settings_emulators(list);
        else if(settings_tab_==1)draw_settings_presentation(list);
        else draw_settings_system(list);
        // A single, context-aware glyph legend at the bottom of Settings.
        // The emulator tab changes the X, triangle and touchpad actions as
        // focus enters its accordion; no contradictory duplicate text row.
        if(settings_tab_==0){
            draw_emulator_button_legend(list);
        }else{
            const ui::Hint settings_hints[] = {
                {ui::Button::l1,hint("Tabs"),ui::Button::r1},
                {ui::Button::dpad,hint("Auswahl")},
                {ui::Button::cross,hint("Aendern")},
                {ui::Button::circle,hint("Schliessen")}
            };
            ui::HintLayout settings_layout;
            settings_layout.cy=982;
            settings_layout.size=24;
            settings_layout.text_size=19;
            settings_layout.item_gap=34;
            ui::draw_hints(list,fonts,ui::GlyphStyle::dark(),settings_hints,4,
                195,false,settings_layout);
        }
    }
    void draw_root_button_legend(gfx::DrawList &list) const {
        const ui::Hint hints[]={
            {ui::Button::l1,hint("Tabs"),ui::Button::r1},
            {ui::Button::l2,hint("Filter"),ui::Button::r2},
            {ui::Button::cross,hint("AN/AUS")},
            {ui::Button::square,hint("Scan")},
            {ui::Button::triangle,hint("Hinzufuegen")},
            {ui::Button::touchpad,hint("Bearbeiten")},
            {ui::Button::circle,hint("Schliessen")}
        };
        ui::HintLayout layout;
        layout.cy=983;layout.size=19;layout.text_size=15;
        layout.item_gap=16;
        ui::draw_hints(list,context_.fonts,ui::GlyphStyle::dark(),hints,7,
            194,false,layout);
    }
    void draw_emulator_button_legend(gfx::DrawList &list) const {
        const auto &fonts=context_.fonts;
        const bool detail=emulator_expanded_ && emulator_subselected_>=0;
        const auto profiles=emulator_profile_indices();
        bool app=false,folder=false,add=false;
        if(detail && !profiles.empty() && emulator_selected_>=0 &&
           emulator_selected_<static_cast<int>(profiles.size())){
            const auto &p=platform_registry_.entries[profiles[static_cast<std::size_t>(emulator_selected_)]];
            app=emulator_subselected_==0;
            add=emulator_subselected_==static_cast<int>(p.rom_folders.size())+1;
            folder=!app && !add;
        }
        // All enumerated buttons exist in ps5-homebrew-ui's ui/glyphs.hpp.
        // Touchpad and both triggers are actual controller glyphs, not text.
        const ui::Hint common[]={
            {ui::Button::l1,hint("Tabs"),ui::Button::r1},
            {ui::Button::dpad,hint("Auswahl")}
        };
        ui::Hint hints[9]{};
        int count=0;
        for(const auto &entry:common)hints[count++]=entry;
        if(detail){
            hints[count++]={ui::Button::cross,hint(app?"Anwendung bearbeiten":
                add?"Ordner hinzufuegen":"Pfad bearbeiten")};
            hints[count++]={ui::Button::touchpad,hint("Pfad eingeben")};
            if(folder){
                hints[count++]={ui::Button::triangle,hint("Entfernen")};
                if(!profiles.empty() &&
                   platform_registry_.entries[profiles[static_cast<std::size_t>(emulator_selected_)]].scan_supported)
                    hints[count++]={ui::Button::left_stick,hint("Scan AN/AUS")};
            }
            hints[count++]={ui::Button::square,hint(app?"Menue testen":"Einklappen")};
        }else{
            hints[count++]={ui::Button::cross,hint("AN/AUS")};
            hints[count++]={ui::Button::square,hint(emulator_expanded_?"Einklappen":"Aufklappen")};
        }
        hints[count++]={ui::Button::l2,hint("Reihenfolge"),ui::Button::r2};
        hints[count++]={ui::Button::circle,hint("Schliessen")};
        ui::HintLayout layout;
        layout.cy=983;
        layout.size=19;
        layout.text_size=15;
        layout.item_gap=12;
        ui::draw_hints(list,fonts,ui::GlyphStyle::dark(),hints,count,
            192,false,layout);
    }
    void draw_settings_emulators(gfx::DrawList &list) const {
        const auto &fonts=context_.fonts;
        translated_text(list,fonts.display,"Emulatoren & Spielordner verwalten",188,289,30,kInk);
        translated_text(list,fonts.regular,
            "Alle Konsolen sortieren, Spieleordner verwalten und Scanpfade einzeln aktivieren.",190,323,19,kAccent);
        const auto ordered=emulator_profile_indices();
        const int count=static_cast<int>(ordered.size());
        const float list_top=339.0f;
        const int visible=8;
        const int first=emulator_expanded_?emulator_selected_:
            std::clamp(emulator_selected_-visible+1,0,std::max(0,count-visible));
        float y=list_top;
        for(int n=first;n<count && y<884.0f;++n){
            const auto &p=platform_registry_.entries[ordered[static_cast<std::size_t>(n)]];
            const bool selected=n==emulator_selected_;
            const float row_h=59.0f;
            const Rect row{187,y,1531,row_h-4};
            list.rounded_rect(row,12,selected?Color::rgb(0x22585A,0.86f):Color::rgb(0x202E40,0.79f));
            if(selected)list.bordered_rect(row,12,Color::rgb(0xFFFFFF,0),1.2f,
                                           Color::rgb(0x7CE0C8,0.68f));
            const auto icon=covers_.get("/app0/assets/platforms/"+p.icon_path);
            if(icon)list.image(icon,{199,y+7,39,39},gfx::kFullUv,
                Color::rgb(0xFFFFFF,p.enabled?1.0f:0.5f),0);
            translated_text(list,fonts.semibold,p.enabled?"AN":"AUS",253,y+35,18,
                            p.enabled?Color::rgb(0x98E8CC):kMuted);
            translated_text(list,fonts.semibold,p.display_name,314,y+35,21,kInk);
            translated_text(list,fonts.regular,p.is_emulator?application_name(p):"Native Konsole",
                            780,y+34,17,kMuted);
            translated_text(list,fonts.regular,
                std::to_string(p.rom_folders.size())+" Ordner",1324,y+34,18,kAccent);
            translated_text(list,fonts.semibold,selected && emulator_expanded_?"v":" >",
                            1650,y+35,20,kAccent);
            y+=row_h;
            if(!selected || !emulator_expanded_)continue;
            // Accordion: the app identity always stays immediately below the
            // platform row, then a scrollable window of roots and an add row.
            const int sub_count=static_cast<int>(p.rom_folders.size())+2;
            int sub_start=0;
            if(emulator_subselected_>4)sub_start=emulator_subselected_-4;
            const int sub_shown=std::min(6,sub_count-sub_start);
            for(int sub=sub_start;sub<sub_start+sub_shown && y+75.0f<892.0f;++sub){
                const bool focus=emulator_subselected_==sub;
                const Rect detail{227,y+2,1443,69};
                list.rounded_rect(detail,12,focus?Color::rgb(0x244E50,0.86f):Color::rgb(0x101E29,0.86f));
                if(focus)list.bordered_rect(detail,12,Color::rgb(0xFFFFFF,0),1.1f,
                                            Color::rgb(0x7CE0C8,0.64f));
                if(sub==0){
                    translated_text(list,fonts.semibold,"Emulator-Anwendung: "+application_name(p),249,y+29,20,kInk);
                    translated_text(list,fonts.regular,
                        "Titel-ID: "+(!emulator_detection_for(p.id).title_id.empty()?
                            emulator_detection_for(p.id).title_id:
                            (p.title_id.empty()?std::string("Nicht eingerichtet"):p.title_id))+
                        "    | Installationsordner: "+shorten(p.app_path.empty()?"nicht erforderlich / optional":p.app_path,43),
                        249,y+56,17,kMuted);
                }else if(sub==sub_count-1){
                    translated_text(list,fonts.semibold,"+ Weiteren Spieleordner hinzufuegen",250,y+42,21,kAccent);
                }else{
                    const auto &folder=p.rom_folders[static_cast<std::size_t>(sub-1)];
                    translated_text(list,fonts.semibold,"Spieleordner "+std::to_string(sub),250,y+29,18,kInk);
                    translated_text(list,fonts.regular,shorten(folder,91),250,y+55,17,kMuted);
                    if(p.scan_supported){
                        translated_text(list,fonts.semibold,
                            profile_folder_enabled(p,static_cast<std::size_t>(sub-1))?"AN":"AUS",
                            1560,y+55,16,kAccent);
                    }
                    if(focus){
                        ui::draw_button(list,fonts,ui::GlyphStyle::dark(),ui::Button::triangle,
                                        1422,y+24,22);
                        translated_text(list,fonts.regular,"Entfernen",1455,y+30,16,kAccent);
                    }
                }
                y+=75.0f;
            }
            if(sub_count>6)translated_text(list,fonts.regular,
                "Weitere Ordner: Hoch/Runter",1140,906,17,kAccent);
        }
        // Button actions are shown only in draw_emulator_button_legend().
        if(!platform_settings_message_.empty())translated_text(list,fonts.semibold,
            shorten(platform_settings_message_,105),193,933,16,kAccent);
    }
    void draw_settings_roots(gfx::DrawList &list) const {
        const auto &fonts=context_.fonts;
        unsigned active=0;
        for(const auto &r:roots_)if(r.enabled)++active;
        const std::string stats="Aktiv: "+std::to_string(active)+" / "+std::to_string(roots_.size())+
            "     Spiele: "+std::to_string(counts_[0])+"     Emulatoren: "+std::to_string(counts_[6]);
        translated_text(list,fonts.semibold,stats,185,287,23,Color::rgb(0xA6DBF7));
        const std::string filter=settings_filter_==0?"Alle Plattformen":
            kPlatforms[static_cast<std::size_t>(settings_filter_-1)];
        translated_text(list,fonts.semibold,"Filter: "+filter+" (L2/R2)",185,332,22,kAccent);
        translated_text(list,fonts.regular,"X: Ordner aktivieren / deaktivieren",900,332,19,kMuted);
        const auto visible_roots=settings_visible_roots();
        constexpr int shown=9;
        const auto selected_it=std::find(visible_roots.begin(),visible_roots.end(),
                                        static_cast<std::size_t>(settings_selected_));
        const int cursor=selected_it==visible_roots.end()?0:static_cast<int>(selected_it-visible_roots.begin());
        const int first=std::max(0,cursor-shown+1);
        for(int row=first;row<static_cast<int>(visible_roots.size()) && row<first+shown;++row){
            const int i=static_cast<int>(visible_roots[static_cast<std::size_t>(row)]);
            const float y=364+static_cast<float>(row-first)*56;
            const auto &root=roots_[static_cast<std::size_t>(i)];
            const ulg::RootState state=i<static_cast<int>(catalogue_.roots.size()) ?
                catalogue_.roots[static_cast<std::size_t>(i)] : ulg::probe_root(root);
            const bool selected=i==settings_selected_;
            list.rounded_rect({182,y,1548,51},12,selected?Color::rgb(0x475D8F):Color::rgb(0x242E42));
            const bool enabled=root.enabled;
            list.rounded_rect({194,y+10,94,30},8,enabled?Color::rgb(0x236A55):Color::rgb(0x4B5366));
            translated_text(list,fonts.semibold,enabled?"AN":"AUS",216,y+31,17,
                enabled?Color::rgb(0xC9FFE6):kMuted);
            translated_text(list,fonts.semibold,root.platform,301,y+33,18,platform_color(root.platform));
            translated_text(list,fonts.regular,shorten(root.path,52),466,y+33,17,kInk);
            if(enabled)translated_text(list,fonts.semibold,state_label(state),1320,y+32,17,
                (state.status=="ok"||state.status=="ftp_ok")?Color::rgb(0x8ADDAC):Color::rgb(0xF6B17C));
        }
        if(visible_roots.empty())translated_text(list,fonts.regular,
            "Keine Ordner in dieser Plattform. Dreieck: Ordner hinzufuegen.",225,409,23,kMuted);
        if(visible_roots.size()>shown)translated_text(list,fonts.regular,"Ordner "+std::to_string(cursor+1)+
            " / "+std::to_string(visible_roots.size()),1500,891,18,kMuted);
        if(active==0)translated_text(list,fonts.semibold,
            "Alle Ordner sind AUS: Die Spielebibliothek bleibt leer.",190,893,20,Color::rgb(0xF6B17C));
        else translated_text(list,fonts.regular,
            "Nur AKTIVE Ordner werden gelesen; offline USB-Ordner bleiben erkennbar.",190,893,18,kInk);
        if(!settings_message_.empty())translated_text(list,fonts.semibold,
            shorten(settings_message_,92),190,923,18,Color::rgb(0xFFC18D));
        // Native root-tab glyphs are rendered once by draw_root_button_legend().
    }
    void draw_settings_presentation(gfx::DrawList &list) const {
        const auto &fonts=context_.fonts;
        translated_text(list,fonts.display,"Darstellung & Verhalten",189,292,34,kInk);
        translated_text(list,fonts.regular,
            "Persoenliche Optionen, ohne Eingriff in Spielordner oder Verlauf.",190,326,19,kMuted);
        const auto card=[&](int index,float y,const char *heading){
            list.bordered_rect({185,y,1535,132},18,Color::rgb(0x25344A,0.85f),2.0f,
                presentation_selected_==index?Color::rgb(0x7CE0C8,0.90f):Color::rgb(0xFFFFFF,0.13f));
            translated_text(list,fonts.semibold,heading,215,y+37,26,kInk);
        };
        const auto choice=[&](Rect r,bool active,const std::string &caption){
            list.bordered_rect(r,12,
                active?Color::rgb(0x496D75,0.90f):Color::rgb(0x273549,0.82f),1.5f,
                active?Color::rgb(0x7CE0C8,0.85f):Color::rgb(0xFFFFFF,0.17f));
            translated_text(list,fonts.semibold,caption,r.x+20,r.y+31,19,active?kInk:kMuted);
        };
        card(0,348,"Sprache");
        translated_text(list,fonts.regular,"X: Sprache wechseln",230,445,17,kMuted);
        choice({665,393,476,48},!language_settings_.english,"Deutsch");
        choice({1158,393,476,48},language_settings_.english,"English");
        card(1,488,"Emulator-Startverhalten");
        translated_text(list,fonts.regular,"Nur PS2, Switch und Xbox 360",230,585,17,kMuted);
        choice({665,532,476,48},emulator_boot_settings_.direct_boot,"Spiele direkt starten");
        choice({1158,532,476,48},!emulator_boot_settings_.direct_boot,"Nur Emulator-Menue");
        card(2,628,"Kachel-Design");
        const bool frosted=theme_settings_.frosted_glass;
        choice({665,672,476,48},frosted,"Frosted Petrol (Acryl)");
        choice({1158,672,476,48},!frosted,"Transparent (Klar)");
        card(3,768,"Hintergrundmusik");
        translated_text(list,fonts.regular,"BGH-Musik in Dauerschleife",230,861,17,kMuted);
        choice({665,812,476,48},music_settings_.enabled,"Musik AN");
        choice({1158,812,476,48},!music_settings_.enabled,"Musik AUS");
        // One result line well above the shared controller button legend.
        const std::string message=!music_settings_message_.empty()?music_settings_message_:
            !theme_settings_message_.empty()?theme_settings_message_:
            !language_settings_message_.empty()?language_settings_message_:emulator_settings_message_;
        if(!message.empty())translated_text(list,fonts.regular,
            shorten(message,110),190,932,18,kAccent);
    }
    void draw_settings_system(gfx::DrawList &list) const {
        const auto &fonts=context_.fonts;
        list.rounded_rect({184,269,750,325},18,Color::rgb(0x263447,0.82f));
        list.rounded_rect({954,269,772,325},18,Color::rgb(0x263447,0.82f));
        translated_text(list,fonts.semibold,"Controller-Hilfe",205,314,29,kInk);
        translated_text(list,fonts.regular,"X: Spielen / Menue      Kreis: Zurueck",207,355,20,kMuted);
        translated_text(list,fonts.regular,"L3: Details             Quadrat: Aktualisieren",207,394,20,kMuted);
        translated_text(list,fonts.regular,"Dreieck: Suche          Touchpad: Zuletzt gespielt",207,433,20,kMuted);
        translated_text(list,fonts.regular,"L1/R1: Plattformen (Bibliothek)",207,472,20,kMuted);
        translated_text(list,fonts.regular,"L1/R1: Tabs (hier)       L2/R2: Ordnerfilter",207,511,20,kAccent);
        translated_text(list,fonts.regular,language_settings_.english?
            "Options: Settings   |   Up/Down: Select option":
            "Options: Einstellungen   |   Hoch/Runter: Auswahl",207,550,20,kMuted);
        translated_text(list,fonts.semibold,"Systemstatus",977,314,29,kInk);
        translated_text(list,fonts.regular,"Version: BGH V1.1.9 Emulator-Erkennung",978,357,23,kAccent);
        translated_text(list,fonts.regular,"Spiele: "+std::to_string(counts_[0]),978,401,23,kInk);
        translated_text(list,fonts.regular,"Verlaufseintraege: "+std::to_string(recent_list_.size()),978,445,23,kInk);
        unsigned active=0;for(const auto &r:roots_)if(r.enabled)++active;
        translated_text(list,fonts.regular,"Aktive Scanpfade: "+std::to_string(active)+" / "+std::to_string(roots_.size()),978,489,23,kInk);
        translated_text(list,fonts.regular,"Diagnose: Nur lesen; keine Loesch- oder Reparaturfunktion.",978,541,17,kMuted);
        // Diagnostic reads/writes cover both the download mount and the /data fallback.
        list.rounded_rect({184,614,1540,288},14,Color::rgb(0x0D1927,0.92f));
        const std::string primary_dir_status=primary_storage_visible_?"SICHTBAR":
            "FEHLER errno="+std::to_string(primary_storage_error_);
        const std::string fallback_dir_status=fallback_storage_visible_?"SICHTBAR":
            "FEHLER errno="+std::to_string(fallback_storage_error_);
        translated_text(list,fonts.semibold,"VERLAUF-DIAG  PRIMARY="+std::string(kPrimaryStateFolder)+" ("+primary_dir_status+")   FALLBACK="+
            std::string(kFallbackStateFolder)+" ("+fallback_dir_status+")",201,651,16,kAccent);
        translated_text(list,fonts.regular,shorten("LESEN: primary prefs="+storage_status_label(prefs_read_status_)+
            "  primary recent="+storage_status_label(recent_read_status_)+
            "  fallback prefs="+storage_status_label(fallback_prefs_read_status_)+
            "  fallback recent="+storage_status_label(fallback_recent_read_status_),150),201,690,17,kInk);
        translated_text(list,fonts.regular,shorten("SCHREIBEN (letzter Start): primary prefs="+storage_status_label(prefs_write_status_)+
            "  primary recent="+storage_status_label(recent_write_status_)+
            "  fallback prefs="+storage_status_label(fallback_prefs_write_status_)+
            "  fallback recent="+storage_status_label(fallback_recent_write_status_),150),201,730,17,kInk);
        translated_text(list,fonts.regular,shorten("PROBE primary: schreiben="+storage_status_label(probe_primary_write_status_)+
            "  lesen="+storage_status_label(probe_primary_read_status_)+
            "  aufraeumen="+probe_primary_cleanup_status_,150),201,770,17,kInk);
        translated_text(list,fonts.regular,shorten("PROBE fallback: schreiben="+storage_status_label(probe_fallback_write_status_)+
            "  lesen="+storage_status_label(probe_fallback_read_status_)+
            "  aufraeumen="+probe_fallback_cleanup_status_,150),201,810,17,kInk);
        translated_text(list,fonts.regular,"GESPEICHERTE REFERENZEN="+std::to_string(recent_list_.size())+
            "  PASSENDE KARTEN="+std::to_string(recent_card_count())+
            "  LETZTE ID="+(recent_list_.empty()?"leer":shorten(recent_list_.front(),45)),
            201,864,17,kAccent);
    }
    void draw_browser(app::Frame &frame) const {
        frame.glass=true;auto &list=frame.overlay;const auto &fonts=context_.fonts;
        list.rounded_rect({260,118,1400,828},35,Color::rgb(0x121A2C,0.99f));
        translated_text(list,fonts.display,"Ordnerbrowser (nur lesend)",313,208,40,kInk);
        translated_text(list,fonts.semibold,shorten(browser_path_,75),311,270,22,kAccent);
        translated_text(list,fonts.regular,emulator_path_edit_?
            ("Plattform: "+platform_registry_.entries[emulator_profile_indices()[static_cast<std::size_t>(emulator_selected_)]].display_name):
            ("Plattform: "+std::string(kPlatforms[static_cast<std::size_t>(add_platform_)])),311,312,22,kMuted);
        if(browser_.state.status!="ok" && browser_.state.status!="ftp_ok"){
            translated_text(list,fonts.semibold,"Verzeichnis kann nicht gelesen werden",315,402,27,Color::rgb(0xF6B17C));
            translated_text(list,fonts.regular,"POSIX "+browser_.state.operation+" errno="+std::to_string(browser_.state.error_code),315,442,24,kMuted);
            translated_text(list,fonts.regular,"Pfad alternativ mit Bildschirmtastatur eingeben",315,492,21,kAccent);
        }else if(browser_.entries.empty())translated_text(list,fonts.semibold,"Keine Unterordner vorhanden",315,403,25,kMuted);
        else {
            constexpr int visible=10;
            const int first=std::max(0,browser_selected_-visible+1);
            for(int i=first;i<static_cast<int>(browser_.entries.size()) && i<first+visible;++i){
                const float y=342+static_cast<float>(i-first)*49;
                list.rounded_rect({311,y,1276,45},11,i==browser_selected_?Color::rgb(0x51629F):Color::rgb(0x242E42));
                translated_text(list,fonts.regular,shorten(browser_.entries[static_cast<std::size_t>(i)].name,65),334,y+30,22,kInk);
            }
        }
        // Folder browser: use the same real DualSense glyphs as Settings.
        const ui::Hint browser_hints[]={
            {ui::Button::cross,hint("Ordner oeffnen")},
            {ui::Button::square,hint("Hier hinzufuegen")},
            {ui::Button::touchpad,hint("Nach oben")}
        };
        ui::HintLayout browser_layout;
        browser_layout.cy=865;browser_layout.size=24;
        browser_layout.text_size=18;browser_layout.item_gap=34;
        ui::draw_hints(list,fonts,ui::GlyphStyle::dark(),browser_hints,3,
            313,false,browser_layout);
        const ui::Hint browser_footer[]={
            {ui::Button::triangle,hint("Pfad eingeben")},
            {ui::Button::circle,hint("Zurueck")}
        };
        ui::HintLayout browser_footer_layout;
        browser_footer_layout.cy=909;browser_footer_layout.size=23;
        browser_footer_layout.text_size=18;browser_footer_layout.item_gap=30;
        ui::draw_hints(list,fonts,ui::GlyphStyle::dark(),browser_footer,2,
            313,false,browser_footer_layout);
        if(!emulator_path_edit_){
            const ui::Hint filter_hint[]={{ui::Button::l1,hint("Plattform"),ui::Button::r1}};
            ui::draw_hints(list,fonts,ui::GlyphStyle::dark(),filter_hint,1,
                1030,false,browser_footer_layout);
        }
    }
    void draw_path_editor(app::Frame &frame) const {
        frame.glass=true;auto &list=frame.overlay;const auto &fonts=context_.fonts;
        list.rounded_rect({267,93,1386,899},32,Color::rgb(0x111A2D,0.99f));
        translated_text(list,fonts.display,edited_root_>=0?"Spielpfad bearbeiten":"Spielpfad hinzufuegen",328,166,39,kInk);
        translated_text(list,fonts.regular,emulator_path_edit_?
            ("Plattform: "+platform_registry_.entries[emulator_profile_indices()[static_cast<std::size_t>(emulator_selected_)]].display_name):
            ("Plattform: "+std::string(kPlatforms[static_cast<std::size_t>(add_platform_)])),332,212,23,kAccent);
        list.rounded_rect({321,235,1255,100},12,Color::rgb(0x26314C));
        translated_text(list,fonts.semibold,shorten(input_path_,79),342,294,23,kInk);
        translated_text(list,fonts.regular,path_uppercase_?"GROSSBUCHSTABEN":"kleinbuchstaben",1300,361,20,kAccent);
        const int count=static_cast<int>(std::strlen(kPathKeys));
        for(int i=0;i<count;++i){
            const int col=i%9,row=i/9;
            const float x=325+col*139.0f,y=399+row*91.0f;
            list.rounded_rect({x,y,125,79},12,i==path_key_?Color::rgb(0x647BC1):Color::rgb(0x27344C));
            char letter=kPathKeys[i];
            std::string label;
            if(letter=='<')label="LOESCH";
            else if(letter=='+')label="FERTIG";
            else {if(letter>='A'&&letter<='Z'&&!path_uppercase_)letter=static_cast<char>(letter-'A'+'a');label=letter;}
            translated_text(list,fonts.semibold,label,x+17,y+49,label.size()>1?17:29,kInk);
        }
        const ui::Hint edit_hints[]={
            {ui::Button::cross,hint("Zeichen")},
            {ui::Button::square,hint("Loeschen")},
            {ui::Button::triangle,hint("Gross/Klein")},
            {ui::Button::circle,hint("Zurueck")}
        };
        ui::HintLayout edit_layout;
        edit_layout.cy=945;edit_layout.size=22;
        edit_layout.text_size=17;edit_layout.item_gap=28;
        ui::draw_hints(list,fonts,ui::GlyphStyle::dark(),edit_hints,4,
            323,false,edit_layout);
    }
    app::Context &context_;
    ulg::Catalogue catalogue_;
    std::vector<ulg::Root> roots_;
    ulg::BrowserResult browser_;
    std::string browser_path_="/",input_path_="/",settings_message_;
    int settings_tab_=0; // Settings tab navigation remains transient
    ulg::PlatformRegistry platform_registry_=ulg::default_platform_registry();
    std::vector<ulg::emulator_detection::Profile> emulator_profiles_;
    std::map<std::string,ulg::emulator_detection::Detection> detected_emulators_;
    std::vector<SidebarItem> sidebar_items_;
    std::string platform_settings_message_;
    int emulator_selected_=0;
    bool emulator_path_edit_=false;
    bool emulator_expanded_=false,emulator_edit_app_=false;
    int emulator_subselected_=-1;
    std::size_t emulator_edit_folder_=0;
    int presentation_selected_=1; // Preserve X=emulator switch until user picks language
    ulg::LanguageSettings language_settings_{};
    std::string language_settings_message_;
    ulg::ThemeSettings theme_settings_{};
    ulg::MusicSettings music_settings_{};
    std::string music_settings_message_;
    std::string theme_settings_message_;
    ulg::EmulatorBootSettings emulator_boot_settings_{};
    std::string emulator_settings_message_;
    int settings_selected_=0,settings_filter_=0,add_platform_=3,browser_selected_=0,path_key_=0,edited_root_=-1;
    bool browser_open_=false,path_editor_open_=false,path_uppercase_=false;
    std::vector<std::size_t> indices_;
    std::set<std::string> favorites_;
    ulg::ControllerStatus controllers_;
    float next_controller_poll_=0.0f;
    bool recent_focus_=false;
    int recent_selected_=0;
    std::set<std::string> recent_;
    std::vector<std::string> recent_list_;
    bool recent_save_error_=false;
    ulg::StorageStatus prefs_read_status_,recent_read_status_,prefs_write_status_,recent_write_status_;
    ulg::StorageStatus fallback_prefs_read_status_,fallback_recent_read_status_,fallback_prefs_write_status_,fallback_recent_write_status_;
    ulg::StorageStatus probe_primary_read_status_,probe_primary_write_status_,probe_fallback_read_status_,probe_fallback_write_status_;
    std::string probe_primary_cleanup_status_="not_run",probe_fallback_cleanup_status_="not_run";
    bool primary_storage_visible_=false,fallback_storage_visible_=false;
    int primary_storage_error_=0,fallback_storage_error_=0;
    bool launch_in_progress_=false;
    bool launch_request_accepted_=false;
    float launch_retry_after_=0.0f;
    CoverTextures covers_;
    ulg::CoverLoadDebounce cover_load_debounce_;
    std::set<std::string> attempted_emulator_covers_;
    std::map<std::string,std::string> cover_diagnostics_;
    std::map<std::string,unsigned> bad_cover_reimports_;
    std::string cache_error_;
    float clock_=0;
    // selected_ is the sole game-selection authority. These two floats only
    // animate its visual representation; neither can choose a different game.
    float visual_selection_=0.0f,list_scroll_=0.0f;
    std::array<int,kTabs.size()> counts_{};
    bool sidebar_focus_=false;
    int tab_=0,selected_=0,refresh_count_=0;
    bool detail_=false,settings_=false,search_open_=false;
    std::string search_query_;
    int search_key_=0;
    std::string launch_message_;
    bool local_services_checked_=false;
    ulg::LocalLaunchServices local_services_{};
    ulg::playtime::Summary playtime_log_{};
};
} // namespace
std::unique_ptr<app::Concept> make_universal_launcher(app::Context &context) {
    return std::make_unique<UniversalLauncher>(context);
}
} // namespace hui::concepts
