// SPDX-License-Identifier: MIT
#include "ftp_scan.hpp"
#include "ftp_readonly.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <cctype>
#include <set>
#include <string>
#include <utility>

namespace ulg {
namespace {
bool begins(const std::string &s,const std::string &p){return s.compare(0,p.size(),p)==0;}
bool ends_ci(const std::string &s,const std::string &extension){
    if(s.size()<extension.size())return false;
    for(std::size_t i=0;i<extension.size();++i){
        const unsigned char c=static_cast<unsigned char>(s[s.size()-extension.size()+i]);
        if(std::tolower(c)!=extension[i])return false;
    }
    return true;
}
bool valid_native_id(const std::string &name,const std::string &prefix){
    if(name.size()!=9 || name.compare(0,4,prefix)!=0)return false;
    for(std::size_t i=4;i<9;++i)if(name[i]<'0'||name[i]>'9')return false;
    return true;
}
bool valid_cusa_name(const std::string &name){
    if(name.size()!=9 || !begins(name,"CUSA"))return false;
    for(std::size_t i=4;i<9;++i)if(name[i]<'0'||name[i]>'9')return false;
    return true;
}
std::string suffix(const std::string &s){const auto pos=s.find_last_of('/');return pos==std::string::npos?s:s.substr(pos+1);}
std::string lowercase(std::string s){for(char &c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
bool removable_title_tag(std::string text){
    while(!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))text.erase(text.begin());
    while(!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))text.pop_back();
    if(text.empty())return true;
    const std::string low=lowercase(text);
    if(low.size()>=2 && low[0]=='v' && std::isdigit(static_cast<unsigned char>(low[1])))return true;
    bool hex_like=text.size()>=12;
    for(char c:text)if(!std::isxdigit(static_cast<unsigned char>(c)))hex_like=false;
    if(hex_like)return true;
    if(low.find("ppsa")!=std::string::npos || low.find("cusa")!=std::string::npos)return true;
    static const char *tokens[]={"usa","eur","europe","european","australia","japan","pal","ntsc",
        "world","en","fr","de","es","it","pt","ru","uk","multi","beta","demo","sample",
        "proto","prototype","disc","rev","version","update"};
    bool matched=false;
    std::string word;
    auto flush=[&](){
        if(word.empty())return;
        for(const char *token:tokens)if(word==token){matched=true;word.clear();return;}
        if(word.size()==1 && std::isalpha(static_cast<unsigned char>(word[0]))){matched=true;word.clear();return;}
        if(word.size()==2 && std::isalpha(static_cast<unsigned char>(word[0])) && std::isalpha(static_cast<unsigned char>(word[1]))){matched=true;word.clear();return;}
        if(word.size()>1 && word[0]=='v' && std::all_of(word.begin()+1,word.end(),[](unsigned char c){return std::isdigit(c);})){
            matched=true;word.clear();return;
        }
        matched=false;word.clear();
    };
    for(char c:low){
        if((c>='a'&&c<='z')||(c>='0'&&c<='9'))word+=c;
        else flush();
        if(!matched && word.empty() && !std::isalnum(static_cast<unsigned char>(c)) && c!=',' && c!=' ' && c!='-' )return false;
        if(!matched && word.empty() && std::isalnum(static_cast<unsigned char>(c))==0)matched=true;
    }
    flush();
    return matched;
}
std::string collapse_title_spaces(std::string s){
    std::string out;out.reserve(s.size());bool previous_space=true;
    for(char c:s){const bool space=std::isspace(static_cast<unsigned char>(c));
        if(space){if(!previous_space)out.push_back(' ');}else out.push_back(c);
        previous_space=space;}
    while(!out.empty() && out.back()==' ')out.pop_back();
    return out;
}
std::string title_from_name(std::string s){
    if(ends_ci(s,".ffpfsc"))s.resize(s.size()-7);
    else if(ends_ci(s,".exfat"))s.resize(s.size()-6);
    const auto dot=s.find_last_of('.');if(dot!=std::string::npos && s.size()-dot<=5)s.resize(dot);
    for(char &c:s)if(c=='_'||c=='.')c=' ';
    std::string cleaned;cleaned.reserve(s.size());
    for(std::size_t i=0;i<s.size();){
        const char c=s[i];
        if(c=='['||c=='('){
            const char close=(c=='[')?']':')';
            const auto end=s.find(close,i+1);
            if(end!=std::string::npos){
                const std::string inner=s.substr(i+1,end-i-1);
                if(removable_title_tag(inner)){
                    if(!cleaned.empty() && cleaned.back()!=' ')cleaned.push_back(' ');
                    i=end+1;continue;
                }
            }
        }
        cleaned.push_back(c);++i;
    }
    cleaned=collapse_title_spaces(cleaned);
    while(!cleaned.empty() && (cleaned.back()=='-'||cleaned.back()==','||cleaned.back()==':'))cleaned.pop_back();
    while(!cleaned.empty() && cleaned.front()==' ')cleaned.erase(cleaned.begin());
    cleaned=collapse_title_spaces(cleaned);
    return cleaned.empty()?"Unbenanntes Spiel":cleaned;
}
std::string title_id_from_name(const std::string &path){
    const std::string name=suffix(path);
    for(std::size_t p=0;p+9<=name.size();++p){
        if(name.compare(p,4,"PPSA")!=0 && name.compare(p,4,"CUSA")!=0)continue;
        if(p>0 && std::isalnum(static_cast<unsigned char>(name[p-1])))continue;
        bool ok=true;
        for(std::size_t j=p+4;j<p+9;++j)if(name[j]<'0'||name[j]>'9')ok=false;
        if(p+9<name.size() && std::isdigit(static_cast<unsigned char>(name[p+9])))ok=false;
        if(ok)return name.substr(p,9);
    }
    return {};
}
std::string platform_from_path(const std::string &path,const std::string &fallback){
    const auto id=title_id_from_name(path);
    if(begins(id,"PPSA"))return "PS5";
    if(begins(id,"CUSA"))return "PS4";
    return fallback;
}
std::string format_for_file(const std::string &platform,const std::string &name){
    if((platform=="PS5" || platform=="PS5/PS4") &&
       (ends_ci(name,".ffpfsc") || ends_ci(name,".exfat")) &&
       valid_native_id(title_id_from_name(name),"PPSA"))
        return ends_ci(name,".ffpfsc")?"ffpfsc":"exfat";
    if(platform=="PS2")for(const char *e:{".iso",".chd",".cso",".zso"})if(ends_ci(name,e))return e+1;
    if(platform=="Switch")for(const char *e:{".nsp",".xci"})if(ends_ci(name,e))return e+1;
    if(platform=="Xbox 360"){
        if(ends_ci(name,".iso"))return "iso";
        if(ends_ci(name,"default.xex"))return "xex";
    }
    if(platform=="PS1")for(const char *e:{".cue",".chd"})if(ends_ci(name,e))return e+1;
    if(platform=="PSP")for(const char *e:{".iso",".cso",".chd",".pbp"})if(ends_ci(name,e))return e+1;
    return {};
}
std::string stable_id(const std::string &platform,const std::string &path){
    unsigned long long h=14695981039346656037ull;
    for(unsigned char c:platform){h^=c;h*=1099511628211ull;}
    h^=0;h*=1099511628211ull;
    for(unsigned char c:path){h^=c;h*=1099511628211ull;}
    std::string out(16,'0');const char *hex="0123456789abcdef";
    for(int i=15;i>=0;--i){out[static_cast<std::size_t>(i)]=hex[h&15];h>>=4;}
    return out;
}
void append_game(Catalogue &out,std::set<std::string> &seen,const std::string &platform,
                 const std::string &path,const std::string &title,const std::string &format,
                 std::size_t max_games, const std::string &metadata_id = {}){
    if(out.games.size()>=max_games){out.truncated=true;return;}
    const std::string id=stable_id(platform,path);
    if(!seen.insert(id).second)return;
    Game item;item.id=id;item.title=title;item.platform=platform;item.path=path;
    item.format=format;item.available=true;item.via_ftp=true;
    item.title_id=title_id_from_name(path);
    if(!metadata_id.empty())item.title_id=metadata_id;
    out.games.push_back(std::move(item));
}
// A visible path is not enough on a sandboxed PS5: lstat may succeed while
// fopen fails. A small read-only signature check distinguishes these cases.
bool readable_png(const std::string &path){
    FILE *f=std::fopen(path.c_str(),"rb");if(!f)return false;
    unsigned char bytes[8]{};
    const auto got=std::fread(bytes,1,sizeof(bytes),f);
    std::fclose(f);
    const unsigned char signature[]={137,80,78,71,13,10,26,10};
    return got==8 && std::memcmp(bytes,signature,8)==0;
}
bool readable_native_art(const std::string &path){
    FILE *f=std::fopen(path.c_str(),"rb");
    if(!f)return false;
    unsigned char b[8]{};
    const auto n=std::fread(b,1,sizeof(b),f);
    std::fclose(f);
    const unsigned char png[]={137,80,78,71,13,10,26,10};
    // Do not discard existing JPEG posters: the PS5 image decoder supports
    // them even though FTP artwork recovery stores only verified PNG files.
    return (n==8 && std::memcmp(b,png,8)==0) ||
           (n>=3 && b[0]==0xff && b[1]==0xd8 && b[2]==0xff);
}
bool recovery_needed(const Game &g){
    if(g.cover_path.empty())return true;
    // A PS5 USB virtual mount can expose lstat() but deny fopen(), including
    // /mnt/usb1/homebrew/PPSAxxxxx-app0/sce_sys/icon0.png. Retry via the
    // user's ALREADY RUNNING read-only FTP service in that case.
    if(g.format=="installationsordner" &&
       (g.platform=="PS5" || g.platform=="PS4"))
        return !readable_native_art(g.cover_path);
    return (begins(g.cover_path,"/user/appmeta/") ||
            begins(g.cover_path,"/download0/hui/ulg_v04/covers/")) &&
           !readable_native_art(g.cover_path);
}
// Cache only confirmed PNG images, and only in the launcher's own writable state.
std::string cached_cover(FtpReadonly &ftp,const std::string &remote_path,
                         const std::string &cache_path,std::size_t &image_budget,
                         std::string *diagnostic=nullptr){
    if(cache_path.empty()||image_budget==0){if(diagnostic)*diagnostic="BUDGET/CACHE-PFAD";return {};}
    struct stat st{};
    if(lstat(cache_path.c_str(),&st)==0 && S_ISREG(st.st_mode) && st.st_size>8 &&
       readable_png(cache_path))return cache_path;
    --image_budget;
    std::string bytes;
    if(!ftp.retrieve(remote_path,bytes,5*1024*1024)){
        if(diagnostic)*diagnostic="FTP RETR Antwort="+std::to_string(ftp.last_retr_reply());
        return {};
    }
    const unsigned char png[]={137,80,78,71,13,10,26,10};
    if(bytes.size()<24 || std::memcmp(bytes.data(),png,8)!=0){
        if(diagnostic)*diagnostic="PNG SIGNATUR/BYTES="+std::to_string(bytes.size());
        return {};
    }
    FILE *out=std::fopen((cache_path+".tmp").c_str(),"wb");
    if(!out){if(diagnostic)*diagnostic="BGH CACHE OPEN errno="+std::to_string(errno);return {};}
    const bool written=std::fwrite(bytes.data(),1,bytes.size(),out)==bytes.size();
    const bool closed=std::fclose(out)==0;
    const bool ok=written && closed;
    if(!ok){if(diagnostic)*diagnostic="BGH CACHE WRITE/CLOSE";std::remove((cache_path+".tmp").c_str());return {};}
    if(std::rename((cache_path+".tmp").c_str(),cache_path.c_str())!=0){
        if(diagnostic)*diagnostic="BGH CACHE RENAME errno="+std::to_string(errno);
        std::remove((cache_path+".tmp").c_str());return {};
    }
    return cache_path;
}
void hydrate_game(FtpReadonly &ftp,Game &game,const std::string &cover_dir,
                  std::size_t &metadata_budget,std::size_t &image_budget){
    const bool native=game.format=="installationsordner";
    if(native && metadata_budget>0){
        --metadata_budget;
        std::string json,sfo;
        const bool has_json=ftp.retrieve(game.path+"/sce_sys/param.json",json,262144);
        bool has_sfo=false;
        if(!has_json){
            has_sfo=ftp.retrieve(game.path+"/sce_sys/param.sfo",sfo,262144);
            if(!has_sfo)has_sfo=ftp.retrieve(game.path+"/PS4_GAME/PARAM.SFO",sfo,262144);
        }
        const auto meta=native_metadata_from_bytes(json,sfo);
        game.platform=classify_native_game(game.path,meta,has_json,has_sfo);
        if(!meta.title_id.empty())game.title_id=meta.title_id;
        else if(game.title_id.empty())game.title_id=title_id_from_name(game.path);
        if(!meta.title.empty())game.title=meta.title;
        else if(!game.title_id.empty())game.title="Unbekanntes Spiel ("+game.title_id+")";
        if(game.platform=="Homebrew"){
            if(game.title_id=="PPSA99203")game.title="PS5SX2";
            else if(game.title_id=="PPSA99008")game.title="ProsperoEden";
            else if(game.title_id=="PPSA50011")game.title="PS5X360";
            else if(game.title_id=="PPSA99079")game.title="BGH - BawertL's Game Hub";
        }
        // ID must match final platform as favorites/cache use platform + path.
        game.id=stable_id(game.platform,game.path);
    }
    if(cover_dir.empty())return;
    const std::string file=cover_dir+"/"+game.id+".png";
    struct stat st{};
    if(lstat(file.c_str(),&st)==0 && S_ISREG(st.st_mode) && st.st_size>8){game.cover_path=file;return;}
    // Game-specific cover/poster preferred; native icon0 is just fallback.
    const std::string directory=native?game.path:game.path.substr(0,game.path.find_last_of('/'));
    const std::string file_name=suffix(game.path);
    const auto dot=file_name.find_last_of('.');
    const std::string stem=dot==std::string::npos?file_name:file_name.substr(0,dot);
    std::vector<std::string> names={directory+"/cover.png",directory+"/folder.png",directory+"/poster.png",directory+"/boxart.png"};
    if(!native)names.push_back(directory+"/"+stem+".png");
    if(native && ((game.platform=="PS4" && valid_native_id(game.title_id,"CUSA")) ||
                  (game.platform=="PS5" && valid_native_id(game.title_id,"PPSA")))){
        // /user/appmeta is shared by native installed PS4 and PS5 titles.
        names.insert(names.begin(),"/user/appmeta/"+game.title_id+"/icon0.png");
        if(game.platform=="PS5")names.push_back("/user/app/"+game.title_id+"/sce_sys/icon0.png");
    }
    if(native)names.push_back(directory+"/sce_sys/icon0.png");
    for(const auto &remote:names){
        game.cover_path=cached_cover(ftp,remote,file,image_budget);
        if(!game.cover_path.empty())return;
        if(image_budget==0)return;
    }
}
void ftp_walk(FtpReadonly &ftp,const Root &root,const std::string &path,int level,
              int max_depth,std::size_t max_games,Catalogue &catalogue,
              std::set<std::string>&seen,std::size_t &budget,
              const std::string &cover_dir,std::size_t &metadata_budget,std::size_t &image_budget){
    if(catalogue.truncated || budget==0)return;
    --budget;
    const auto listing=ftp.list(path);
    if(!listing.ok){
        ++catalogue.failed_operations;
        if(catalogue.errors.size()<256)catalogue.errors.push_back(
            listing.operation+" (FTP="+std::to_string(listing.reply)+") "+path);
        return;
    }
    const bool native=root.platform=="PS5/PS4"||root.platform=="PS4"||root.platform=="PS5";
    for(const auto &entry:listing.entries){
        if(catalogue.truncated)break;
        if(entry.name.empty()||entry.name[0]=='.' || entry.name=="$RECYCLE.BIN" ||
           entry.name=="System Volume Information" || entry.name=="$Recycle.Bin")continue;
        const std::string child=path+(path=="/"?"":"/")+entry.name;
        if(child.size()>4000)continue;
        if(native){
            if(!entry.folder){
                if(root.platform!="PS4"){
                    const std::string format=format_for_file(root.platform,entry.name);
                    if(!format.empty())append_game(catalogue,seen,"PS5",child,
                        title_from_name(entry.name),format,max_games);
                }
                continue;
            }
            // The verified /user/app/ root mixes installed PS4 and PS5 applications.
            // Do not descend into unrelated PPSA/system apps when scanning PS4 PKGs.
            if(root.platform=="PS4" && root.path=="/user/app" && level==0 &&
               !valid_cusa_name(entry.name))continue;
            const auto inner=ftp.list(child);
            bool is_app=false;
            if(inner.ok){
                for(const auto &it:inner.entries)
                    if(!it.folder && it.name=="eboot.bin")is_app=true;
                if(!is_app)for(const auto &it:inner.entries)if(it.folder && it.name=="sce_sys")is_app=true;
                if(!is_app && (root.platform=="PS4" || root.platform=="PS5/PS4") && valid_cusa_name(entry.name))
                    for(const auto &it:inner.entries)if(!it.folder && it.name=="app.pkg")is_app=true;
            }
            if(is_app){
                const std::string initial=platform_from_path(child,root.platform);
                append_game(catalogue,seen,initial,child,title_from_name(entry.name),"installationsordner",max_games);
                if(!catalogue.games.empty() && catalogue.games.back().path==child){
                    hydrate_game(ftp,catalogue.games.back(),cover_dir,metadata_budget,image_budget);
                    seen.insert(catalogue.games.back().id);
                }
            }else if(level<max_depth && !(root.platform=="PS4" && root.path=="/user/app" && level==0))
                ftp_walk(ftp,root,child,level+1,max_depth,max_games,catalogue,seen,budget,cover_dir,metadata_budget,image_budget);
            continue;
        }
        if(entry.folder){
            if(level<max_depth)ftp_walk(ftp,root,child,level+1,max_depth,max_games,catalogue,seen,budget,cover_dir,metadata_budget,image_budget);
        }else{
            const std::string fmt=format_for_file(root.platform,entry.name);
            if(!fmt.empty()){
                append_game(catalogue,seen,root.platform,child,
                    (root.platform=="Xbox 360"&&ends_ci(entry.name,"default.xex"))?
                        title_from_name(suffix(path)):title_from_name(entry.name),fmt,max_games);
                if(!catalogue.games.empty() && catalogue.games.back().path==child)
                    hydrate_game(ftp,catalogue.games.back(),cover_dir,metadata_budget,image_budget);
            }
        }
    }
}
}
void supplement_from_local_ftp(Catalogue &catalogue,const std::vector<Root> &roots,
                               int max_depth,std::size_t max_games,int port,
                               const std::string &cover_dir){
    bool need=false,ps4_root=false,native_cover_missing=false;
    for(const auto &r:catalogue.roots)if(r.status!="ok"&&r.status!="deaktiviert"){need=true;break;}
    for(const auto &r:roots)if(r.enabled && r.platform=="PS4" && r.path=="/user/app")ps4_root=true;
    for(const auto &g:catalogue.games)
        if((g.platform=="PS4" && valid_native_id(g.title_id,"CUSA")) ||
           (g.platform=="PS5" && valid_native_id(g.title_id,"PPSA")))
            if(recovery_needed(g))native_cover_missing=true;
    if(!need && !ps4_root && !native_cover_missing)return;
    FtpReadonly ftp(port);
    if(!ftp.connect_local())return;
    std::set<std::string> seen;for(const auto &g:catalogue.games)seen.insert(g.id);
    std::size_t budget=500;
    std::size_t metadata_budget=128, image_budget=128;
    if(!cover_dir.empty())(void)mkdir(cover_dir.c_str(),0700);
    const std::size_t count=std::min(roots.size(),catalogue.roots.size());
    for(std::size_t i=0;i<count;++i){
        if(!roots[i].enabled)continue;
        const bool healthy=catalogue.roots[i].status=="ok";
        const bool ps4_supplement=roots[i].platform=="PS4" && roots[i].path=="/user/app";
        if(healthy && !ps4_supplement)continue;
        const auto listing=ftp.list(roots[i].path,1200);
        if(!listing.ok)continue;
        if(!healthy){
            catalogue.roots[i]={roots[i].platform,roots[i].path,"ftp_ok",0,"FTP MLSD"};
            if(catalogue.failed_roots>0)--catalogue.failed_roots;
            if(catalogue.failed_operations>0)--catalogue.failed_operations;
            // The failed POSIX probe was recovered through FTP and is no longer counted.
            for(auto it=catalogue.errors.begin();it!=catalogue.errors.end();++it){
                if(it->size()>=roots[i].path.size() &&
                   it->compare(it->size()-roots[i].path.size(),roots[i].path.size(),roots[i].path)==0){
                    catalogue.errors.erase(it);break;
                }
            }
        }
        // Scan content recursively. CWD/MLSD are read-only FTP commands.
        ftp_walk(ftp,roots[i],roots[i].path,0,max_depth,max_games,catalogue,seen,budget,cover_dir,metadata_budget,image_budget);
        if(catalogue.truncated || budget==0)break;
    }
    // Keep this budget separate: earlier FTP root scans may exhaust
    // image_budget without affecting already discovered native titles.
    std::size_t recovery_budget=256;
    // Keep registered-title and PS5-container artwork quotas separate. A large
    // set of USB installation folders must not starve the standalone images.
    std::size_t container_cover_budget=128;
    // Recover artwork for games already found by POSIX scanning. A healthy
    // USB root does NOT enter ftp_walk(), so without this second-stage probe
    // both native folder games and the standalone .ffpfsc/.exfat entries can
    // remain permanently uncovered. FTP reads only; writes go to BGH's cache.
    for(auto &game:catalogue.games){
        if(cover_dir.empty() || !recovery_needed(game))continue;
        const bool container=(game.platform=="PS5" &&
                              (game.format=="ffpfsc" || game.format=="exfat"));
        const bool installed=game.format=="installationsordner";
        if(!installed && !container)continue;
        if(!((game.platform=="PS4" && valid_native_id(game.title_id,"CUSA")) ||
             (game.platform=="PS5" && valid_native_id(game.title_id,"PPSA"))))continue;
        const std::string cache=cover_dir+"/"+game.id+".png";
        std::vector<std::string> candidates;
        if(container){
            // A standalone PS5 image has no /sce_sys directory. Match its
            // PPSA title ID to the console's existing app metadata instead.
            // Never inspect or modify the container's contents.
            candidates.push_back("/user/appmeta/"+game.title_id+"/icon0.png");
            candidates.push_back("/user/app/"+game.title_id+"/sce_sys/icon0.png");
        }else{
            // Existing native folder priority is deliberately unchanged.
            if(game.platform=="PS5"){
                for(const char *name:{"sce_sys/icon0.png","sce_sys/ICON0.PNG",
                                      "cover.png","Cover.png","folder.png","poster.png",
                                      "boxart.png","front.png","icon0.png"})
                    candidates.push_back(game.path+"/"+name);
            }
            candidates.push_back("/user/appmeta/"+game.title_id+"/icon0.png");
            if(game.platform=="PS5")
                candidates.push_back("/user/app/"+game.title_id+"/sce_sys/icon0.png");
        }
        // All artwork bytes pass through the existing verified PNG signature,
        // bounded FTP RETR, and atomic BGH-only cache write in cached_cover().
        std::size_t &remaining=container?container_cover_budget:recovery_budget;
        std::size_t title_budget=std::min<std::size_t>(candidates.size(),12);
        std::string diagnostic;
        for(const auto &remote:candidates){
            if(title_budget==0 || remaining==0)break;
            --remaining;
            const auto found=cached_cover(ftp,remote,cache,title_budget,&diagnostic);
            if(!found.empty()){
                game.cover_path=found;
                diagnostic.clear();break;
            }
            // FTP 550 is usually a missing candidate, not proof that the
            // metadata for another game is inaccessible.
            if(catalogue.errors.size()<256 && diagnostic.find("FTP RETR Antwort=550")==std::string::npos)
                catalogue.errors.push_back("COVER "+remote+" "+diagnostic);
        }
        if(game.cover_path.empty() && (title_budget==0 || remaining==0)){
            ++catalogue.failed_operations;
            if(catalogue.errors.size()<256)catalogue.errors.push_back(
                "FTP COVER-SUCHE AUSSTEHEND (BUDGET) "+game.path);
        }
    }
    std::sort(catalogue.games.begin(),catalogue.games.end(),[](const Game &a,const Game &b){
        return a.title==b.title?a.platform<b.platform:a.title<b.title;
    });
}
} // namespace ulg
