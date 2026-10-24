#!/usr/bin/env python3
"""Adapt the checked-out (CURRENT) BGH V1.1.9 source, never ship old C++.

Fail closed when critical hooks have been renamed. A successful GitHub workflow
then packages the actually modified code. Does not touch scanners/launch bridges.
"""
from pathlib import Path
import re, sys

ROOT=Path(__file__).resolve().parents[1]
CPP=ROOT/'native_ps5/universal_launcher.cpp'

def replace_once(s,old,new,label,required=True):
    n=s.count(old)
    if n!=1:
        if required: raise RuntimeError(f'{label}: expected one occurrence of {old!r}, found {n}')
        return s
    return s.replace(old,new,1)

def block(s,func):
    match=re.search(r'\bvoid\s+'+re.escape(func)+r'\s*\(',s)
    if not match: return None
    brace=s.find('{',match.end()); depth=0
    for j in range(brace,len(s)):
        if s[j]=='{':depth+=1
        elif s[j]=='}':
            depth-=1
            if depth==0:return match.start(),j+1
    raise RuntimeError('Unbalanced C++ function '+func)

def patch_block(s,func,callback,required=True):
    bounds=block(s,func)
    if not bounds:
        if required:raise RuntimeError('Missing C++ function '+func)
        return s
    a,b=bounds
    return s[:a]+callback(s[a:b])+s[b:]

def core():
    old=CPP.read_text(encoding='utf-8')
    if 'BGH V1.1.9' not in old:
        raise RuntimeError('Current checkout is NOT confirmed V1.1.9. Refuse to overwrite it.')
    s=old
    s=replace_once(s,'#include "controller_status.hpp"',
        '#include "controller_status.hpp"\n#include "bgh_theme_gallery.hpp"\n#include "bgh_storage_metrics.hpp"\n#include "bgh_nextgen_layout.hpp"', 'Gallery headers')
    # The code always uses a 1920x1080 logical canvas, including on 4K televisions.
    new,n=re.subn(r'constexpr float kFanCentreX\s*=\s*[^;]+;',
        'constexpr float kFanCentreX = 960.0f;',s,count=1)
    if n:s=new
    else:
        # Newer V1.1.9 draw paths may center the selected card in another function.
        # Do not silently claim perfect recentering if there is no known anchor.
        if 'kFanCentreX' not in s:raise RuntimeError('BGH fan center anchor missing')
    # Suppress warnings for constants previously used only in the centre expression.
    for constant in ("kGameListRight","kScreenRight","kFanRightMargin"):
        s=s.replace(f"constexpr float {constant} =",f"[[maybe_unused]] constexpr float {constant} =",1)
    # Keep the user's existing 7-card fan; expand 5 only when an old code path exists.
    s=s.replace('std::array<int,5> candidates{};','std::array<int,7> candidates{};',1)
    s=s.replace('for(int i=center-2;i<=center+2;++i)','for(int i=center-3;i<=center+3;++i)',1)
    s=s.replace('<=2.45f)candidates[size++]','<=3.45f)candidates[size++]',1)
    # Increase distances for both v1.1.8 and late v1.1.9 using bounded changes.
    def spread(m):
        oldval=float(m.group(2))
        return m.group(1)+str(round(oldval*1.17,2))+'f'
    s=re.sub(r'(const float near_distance\s*=\s*[^;]*\*\s*)([0-9]+(?:\.[0-9]+)?)f',spread,s,count=1)
    s=re.sub(r'(const float far_distance\s*=\s*[^;]*\*\s*)([0-9]+(?:\.[0-9]+)?)f',spread,s,count=1)
    # The old detector is no longer called after the banner is disabled.
    s=s.replace('bool fan_cover_uses_case_header(', '[[maybe_unused]] bool fan_cover_uses_case_header(',1)
    s=s.replace('depth>1.0f?0.84f:1.0f','depth>1.0f?0.70f:1.0f',1)
    # Remove banner WITHOUT deleting other cover rendering, metadata or source art.
    if 'const bool with_case_header=fan_cover_uses_case_header(g.platform);' in s:
        s=s.replace('const bool with_case_header=fan_cover_uses_case_header(g.platform);',
            'const bool with_case_header=false; // V1.1.10: clean full artwork',1)
    else:
        # In next-gen branches the header is sometimes drawn directly.
        # It is essential to make sure there's no synthetic header left.
        if 'draw_platform_banner(' in s:
            raise RuntimeError('New cover-banner rendering path needs adapting')
    # The latest version may use a different tab structure; fail safely if so.
    tab='"Emulatoren & Spielordner", "Darstellung & Verhalten", "System & Hilfe"'
    s=replace_once(s,tab,tab+', "Hintergruende & Logos"','Settings titles')
    s=replace_once(s,'std::array<const char *,3> kSettingsTabs',
                   'std::array<const char *,4> kSettingsTabs','Settings tab count')
    # Fit four clickable tabs inside 1920 with a safe 184 px left margin.
    s=s.replace('const float x=184+static_cast<float>(i)*520.0f;',
                'const float x=184+static_cast<float>(i)*385.0f;',1)
    s=s.replace('const Rect tab_rect{x,171,514,62};','const Rect tab_rect{x,171,379,62};',1)
    s=replace_once(s,'        else draw_settings_system(list);',
         '        else if(settings_tab_==2)draw_settings_system(list);\n        else draw_settings_theme_gallery(list);',
         'Theme tab renderer dispatch')
    # Add state and storage helpers inside the class (without any changes to play history).
    marker='class UniversalLauncher final : public app::Concept {'
    s=replace_once(s,marker,marker+'''
    ulg::bgh_gallery::Selection gallery_selection_{"mountain_crown","none"};
    mutable ulg::bgh_storage::Volume gallery_volumes_[2];
    mutable float gallery_next_storage_poll_=-1.0f;
    int gallery_row_=0;
    bool gallery_delete_armed_=false;
    std::string gallery_message_;
    std::string gallery_background_cache_="/app0/assets/themes/backgrounds/bgh_berg_cyber_mit_kronenlogo.png";
    std::string gallery_logo_cache_;
    void refresh_gallery_paths(){
        ulg::bgh_gallery::ThemeItem v;
        if(ulg::bgh_gallery::find(ulg::bgh_gallery::Kind::background,gallery_selection_.background,v))
            gallery_background_cache_=v.path;
        else gallery_background_cache_="/app0/assets/premium_lake_background.png";
        if(ulg::bgh_gallery::find(ulg::bgh_gallery::Kind::logo,gallery_selection_.logo,v))
            gallery_logo_cache_=v.path;
        else gallery_logo_cache_.clear();
    }
    const std::string& gallery_background_path() const {return gallery_background_cache_;}
    const std::string& gallery_logo_path() const {return gallery_logo_cache_;}
    void save_gallery(){
        refresh_gallery_paths();
        const bool a=ulg::bgh_gallery::save_selection("/download0/hui/ulg_v04/gallery.cfg",gallery_selection_);
        const bool b=ulg::bgh_gallery::save_selection("/data/bgh/gallery.cfg",gallery_selection_);
        gallery_message_=(a||b)?"Design gespeichert":"Design konnte nicht gespeichert werden";
    }
''','Gallery state')
    # After the existing storage initialization, load new preference separately.
    init='        refresh_storage_directory_status();'
    if init not in s: raise RuntimeError("No constructor storage init")
    s=s.replace(init,init+'''
        if(!ulg::bgh_gallery::load_selection("/download0/hui/ulg_v04/gallery.cfg",gallery_selection_))
            (void)ulg::bgh_gallery::load_selection("/data/bgh/gallery.cfg",gallery_selection_);
        refresh_gallery_paths();
''',1)
    # Do not change the old proven settings behavior of its first three tabs.
    legacy='        if(settings_tab_!=3)return; // Legacy folder tab retired; preserve its helpers for compatibility.'
    s=replace_once(s,legacy,'''        if(settings_tab_==3){
            if(input.nav==Direction::up){gallery_row_=std::max(0,gallery_row_-1);gallery_delete_armed_=false;return;}
            if(input.nav==Direction::down){gallery_row_=std::min(1,gallery_row_+1);gallery_delete_armed_=false;return;}
            const auto kind=gallery_row_==0?ulg::bgh_gallery::Kind::background:ulg::bgh_gallery::Kind::logo;
            if(input.is_pressed(Action::confirm)){
                const auto choices=ulg::bgh_gallery::list(kind);
                const std::string selected=gallery_row_==0?gallery_selection_.background:gallery_selection_.logo;
                int pos=-1;
                for(std::size_t i=0;i<choices.size();++i)
                    if(choices[i].id==selected){pos=static_cast<int>(i);break;}
                if(!choices.empty()){
                    ulg::bgh_gallery::select(kind,choices[static_cast<std::size_t>((pos+1)%static_cast<int>(choices.size()))].id,gallery_selection_);
                    save_gallery();
                }
                gallery_delete_armed_=false;
                return;
            }
            if(input.is_pressed(Action::north)){
                const std::string id=gallery_row_==0?gallery_selection_.background:gallery_selection_.logo;
                if(id.rfind("user:",0)!=0){gallery_message_="Vorinstallierte Motive sind geschuetzt";gallery_delete_armed_=false;return;}
                if(!gallery_delete_armed_){gallery_message_="Dreieck erneut: eigenes Bild loeschen";gallery_delete_armed_=true;return;}
                const auto prev=gallery_selection_;
                if(gallery_row_==0)gallery_selection_.background="showroom";
                else gallery_selection_.logo="none";
                const bool deleted=ulg::bgh_gallery::remove_user_image(kind,id,gallery_selection_,true);
                if(!deleted)gallery_selection_=prev;
                else save_gallery();
                gallery_message_=deleted?"Eigenes Bild geloescht":"Loeschen fehlgeschlagen";
                gallery_delete_armed_=false;
                return;
            }
            return;
        }
        if(settings_tab_!=4)return; // Legacy folder tab retired; preserve its helpers for compatibility.''','Gallery settings input')
    # Add the theme gallery rendering before System & Hilfe.
    s=replace_once(s,'    void draw_settings_system(gfx::DrawList &list) const {','''    void draw_settings_theme_gallery(gfx::DrawList &list) const {
        const auto& fonts=context_.fonts;
        translated_text(list,fonts.display,"Hintergruende & Logos",190,293,35,kInk);
        translated_text(list,fonts.regular,"X: Motiv wechseln  |  Dreieck zweimal: eigenes Motiv loeschen",190,334,21,kMuted);
        const auto backgrounds=ulg::bgh_gallery::list(ulg::bgh_gallery::Kind::background);
        const auto logos=ulg::bgh_gallery::list(ulg::bgh_gallery::Kind::logo);
        const auto label=[](const std::vector<ulg::bgh_gallery::ThemeItem>& choices,const std::string& id){
            for(const auto& item:choices)if(item.id==id)return item.id;
            return std::string("Nicht verfuegbar");
        };
        for(int i=0;i<2;++i){
            const float y=392.0f+static_cast<float>(i)*160.0f;
            list.bordered_rect({185,y,1540,135},18,Color::rgb(0x273A52,0.89f),2.0f,
                i==gallery_row_?Color::rgb(0x06D5FF):Color::rgb(0xFFFFFF,0.13f));
            const bool bg=i==0;
            translated_text(list,fonts.semibold,bg?"Hintergrundbild":"BGH Logo",222,y+51,31,kInk);
            translated_text(list,fonts.semibold,label(bg?backgrounds:logos,
                bg?gallery_selection_.background:gallery_selection_.logo),775,y+53,25,kInk);
        }
        translated_text(list,fonts.regular,"Eigene Bilder per FTP nach /data/bgh/themes/backgrounds oder /data/bgh/themes/logos",190,757,18,kMuted);
        translated_text(list,fonts.regular,gallery_message_,190,847,19,Color::rgb(0x79D8FF));
    }
    void draw_settings_system(gfx::DrawList &list) const {''','Gallery settings renderer')
    # Dynamic image decoding only on an asset miss; no bulk reloads per frame.
    def theme_update(m):
        b=m
        b=b.replace('if(!covers_.has("/app0/assets/bgh_logo.png"))covers_.load("/app0/assets/bgh_logo.png");',
                    'const auto new_logo=gallery_logo_path(); if(!new_logo.empty() && !covers_.has(new_logo))covers_.load(new_logo);')
        b=b.replace('if(!covers_.has("/app0/assets/premium_lake_background.png"))covers_.load("/app0/assets/premium_lake_background.png");',
                    'const auto new_background=gallery_background_path(); if(!covers_.has(new_background))covers_.load(new_background);')
        return b
    s=patch_block(s,'update',theme_update,True)
    needle='const auto new_background=gallery_background_path(); if(!covers_.has(new_background))covers_.load(new_background);'
    if needle not in s:raise RuntimeError('Gallery background preloader missing')
    preload='''
        for(const char* storage_icon:{"/app0/assets/themes/storage_internal.png","/app0/assets/themes/storage_usb.png"})
            if(!covers_.has(storage_icon))covers_.load(storage_icon);'''
    s=s.replace(needle,needle+preload,1)
    # Replace only image getters; leave icon and original wordmark alone.
    s=s.replace('covers_.get("/app0/assets/premium_lake_background.png")','covers_.get(gallery_background_path())',1)
    s=s.replace('covers_.get("/app0/assets/bgh_logo.png")','covers_.get(gallery_logo_path())',1)
    s=s.replace('if(brand)list.image(brand,','if(brand && ulg::bgh_gallery::logo_should_render(gallery_selection_))list.image(brand,',1)
    # Avoid visual double-logo on branded mountain background.
    s=s.replace('if(wordmark)list.image(wordmark,','if(wordmark && ulg::bgh_gallery::logo_should_render(gallery_selection_))list.image(wordmark,',1)
    s=s.replace('else serif_text(list,"BawertL\'s Game Hub",','else if(ulg::bgh_gallery::logo_should_render(gallery_selection_))serif_text(list,"BawertL\'s Game Hub",',1)
    # Storage: draw live values and progress on top of existing placeholder widgets.
    # Always poll at a modest rate via clock; no extra polling in this overlay.
    if not block(s,'draw_storage_widgets'):
        raise RuntimeError('Wrong source branch: NextGen draw_storage_widgets missing')
    if block(s,'draw_storage_widgets'):
        def storage_overlay(b):
            addition='''
        if(gallery_next_storage_poll_<0.0f || clock_>=gallery_next_storage_poll_){
            gallery_volumes_[0]=ulg::bgh_storage::internal();
            gallery_volumes_[1]=ulg::bgh_storage::external();
            gallery_next_storage_poll_=clock_+10.0f;
        }
        for(int i=0;i<2;++i){
            const auto& v=gallery_volumes_[static_cast<std::size_t>(i)];
            const float x=1515.0f+static_cast<float>(i)*193.0f;
            list.rounded_rect({x,205.0f,183.0f,80.0f},11,Color::rgb(0x102138,0.96f));
            const auto storage_icon=covers_.get(i==0?"/app0/assets/themes/storage_internal.png":"/app0/assets/themes/storage_usb.png");
            if(storage_icon)list.image(storage_icon,{x+148,210,24,24},gfx::kFullUv,Color::rgb(0xB9EFFF),0);
            translated_text(list,context_.fonts.semibold,i==0?"Interner Speicher":"Externer USB",x+9,227,15,Color::rgb(0xE6F5FF));
            translated_text(list,context_.fonts.regular,ulg::bgh_storage::label(v),x+9,253,12,Color::rgb(0xDAE7F8));
            list.rounded_rect({x+9,266,162,5},2,Color::rgb(0x33506C));
            if(v.known && v.capacity_bytes){
                const float ratio=std::clamp(static_cast<float>(v.percent()/100.0),0.0f,1.0f);
                if(ratio>0)list.gradient_rect({x+9,266,162*ratio,5},2,Color::rgb(0x0055FF),Color::rgb(0x00D5FF));
            }
        }
'''
            return b[:-1]+addition+'\n}'
        s=patch_block(s,'draw_storage_widgets',storage_overlay)
    # Recent card heading collision: only remove independent second heading.
    s=re.sub(r'^.*(?:Letzte Spiellaufzeit|Startauftraege – kein Nachweis der Spiel-Laufzeit|Startaufträge – kein Nachweis der Spiel-Laufzeit).*$','',s,flags=re.MULTILINE)
    # Keep a visible version string; no false hardware validation claims.
    s=s.replace('BGH V1.1.9-EMULATOR-AUTODETECTION','BGH V1.1.10-NEXTGEN-UI-POLISH')
    s=s.replace('release_label="BGH V1.1.9"','release_label="BGH V1.1.10"')
    s=s.replace('BGH V1.1.9','BGH V1.1.10')
    if s==old:raise RuntimeError('No C++ code changes')
    CPP.write_text(s,encoding='utf-8')
    print('BGH V1.1.10: current C++ patched; launcher bridges and scanner files unchanged')

def controllers():
    # This only marks a secondary controller after verified scePadReadState.
    # No success is claimed for unidentified guest-controller APIs.
    p=ROOT/'native_ps5/controller_status.hpp'
    s=p.read_text(encoding='utf-8')
    anchor='\n        }\n#endif\n    }\nprivate:'
    if anchor not in s:
        raise RuntimeError('Unexpected controller_status.hpp: refuse unverified patch')
    extra=(
        '\n        // Try additional pad ports for guests sharing the primary login.\n'
        '        // Only a successful connected byte makes a slot active.\n'
        '        if(users[0]>=0 && get_ && read_){\n'
        '            for(unsigned i=1;i<4;++i){\n'
        '                if(state[i]==1)continue;\n'
        '                const int h=get_(users[0],0,static_cast<int>(i));\n'
        '                if(h<0)continue;\n'
        '                alignas(8) unsigned char sample[120]{};\n'
        '                if(read_(h,sample)!=0)continue;\n'
        '                std::int32_t connected=0;\n'
        '                std::memcpy(&connected,sample+0x4c,sizeof(connected));\n'
        '                if(connected!=0)state[i]=1;\n'
        '            }\n'
        '        }\n'
    )
    s=s.replace(anchor,'\n        }\n'+extra+'#endif\n    }\nprivate:',1)
    p.write_text(s,encoding='utf-8')

def buildscript():
    p=ROOT/'tooling/build_native_ps5.sh'
    s=p.read_text(encoding='utf-8')
    s=re.sub(r'V1_1_9','V1_1_10',s)
    s=re.sub(r'V1\.1\.9','V1.1.10',s)
    # Make sure a real new artifact name will be generated.
    if 'BGH_BawertLs_Game_Hub_V1_1_10_${TITLE_ID}_PS5_TEST.zip' not in s:
        raise RuntimeError('build script package name not upgraded; abort')
    # Ensure themes are actually copied into the packaged /app0/assets directory.
    anchor='python3 "$PROJECT/tooling/install_ui_kit_overlay.py" "$KIT"'
    if anchor not in s:raise RuntimeError('Missing pinned kit overlay installation')
    s=s.replace(anchor,anchor+'''
# BGH V1.1.10: package the selectable images, not just the default assets.
mkdir -p "$KIT/assets/themes"
cp -a "$PROJECT/assets/themes/." "$KIT/assets/themes/"
# The kit sources compile inside src/concepts, not native_ps5.
for addon_header in bgh_theme_gallery.hpp bgh_storage_metrics.hpp bgh_nextgen_layout.hpp; do
  cp "$PROJECT/native_ps5/$addon_header" "$KIT/src/concepts/$addon_header"
done''',1)
    p.write_text(s,encoding='utf-8')

if __name__=='__main__':
    try:
        core();controllers();buildscript()
    except Exception as exc:
        print('BGH V1.1.10 SOURCE PATCH FAILED:',str(exc),file=sys.stderr)
        sys.exit(1)
