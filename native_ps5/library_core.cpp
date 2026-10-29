// SPDX-License-Identifier: MIT
// No PS5 SDK required. opendir/readdir/lstat/open/read only during scanning.
// No symlink traversal. Limits for recursion, records and unusually long paths.
#include "library_core.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <set>
#include <map>
#include <cctype>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <utility>

namespace ulg {
namespace {
struct Child { std::string name, path; bool folder; };
std::string guess_id(const std::string &path);
bool valid_native_id(const std::string &id,const std::string &prefix);
std::string lowercase(std::string s) {
    for (char &c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return s;
}
bool ends(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(),suffix.size(),suffix)==0;
}
std::string basename(const std::string &s) {
    auto pos = s.find_last_of('/'); return pos==std::string::npos ? s : s.substr(pos+1);
}
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
    static const char *tokens[]={
        "usa","eur","europe","european","australia","japan","japanese","pal","ntsc",
        "world","en","fr","de","es","it","pt","ru","us","uk","multi","beta",
        "demo","sample","proto","prototype","disc","rev","version","update"
    };
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
        matched=false;
        word.clear();
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
    std::string out;out.reserve(s.size());
    bool previous_space=true;
    for(char c:s){
        const bool space=std::isspace(static_cast<unsigned char>(c));
        if(space){
            if(!previous_space)out.push_back(' ');
        }else out.push_back(c);
        previous_space=space;
    }
    while(!out.empty() && out.back()==' ')out.pop_back();
    return out;
}
std::string title_from(std::string s) {
    std::string low=lowercase(s);
    static const char *extensions[]={".iso",".chd",".cso",".zso",".nsp",".xci",".xex",".cue",".pbp",".ffpfsc",".exfat"};
    for (const char *ext:extensions) if (ends(low,ext)) {s.resize(s.size()-std::strlen(ext));break;}
    std::string normalized;normalized.reserve(s.size());
    for (char c:s) normalized.push_back((c=='_' || c=='.')?' ':c);
    std::string cleaned;cleaned.reserve(normalized.size());
    for(std::size_t i=0;i<normalized.size();){
        const char c=normalized[i];
        if(c=='[' || c=='('){
            const char close=(c=='[')?']':')';
            const auto end=normalized.find(close,i+1);
            if(end!=std::string::npos){
                const std::string inner=normalized.substr(i+1,end-i-1);
                if(removable_title_tag(inner)){
                    if(!cleaned.empty() && cleaned.back()!=' ')cleaned.push_back(' ');
                    i=end+1;continue;
                }
            }
        }
        cleaned.push_back(c);++i;
    }
    cleaned=collapse_title_spaces(cleaned);
    while(!cleaned.empty() && (cleaned.back()=='-' || cleaned.back()==',' || cleaned.back()==':'))cleaned.pop_back();
    while(!cleaned.empty() && cleaned.front()==' ')cleaned.erase(cleaned.begin());
    cleaned=collapse_title_spaces(cleaned);
    if (cleaned.empty()) return "Unbenanntes Spiel";
    return cleaned;
}
std::string extension(const std::string &s) {
    auto pos=s.find_last_of('.');return pos==std::string::npos?"":lowercase(s.substr(pos));
}
std::string format_for(const std::string &platform,const std::string &path) {
    const std::string e=extension(path);
    if((platform=="PS5" || platform=="PS5/PS4") && (e==".ffpfsc" || e==".exfat") &&
       valid_native_id(guess_id(path),"PPSA"))return e.substr(1);
    if(platform=="PS2" && (e==".iso" || e==".chd" || e==".cso" || e==".zso")) return e.substr(1);
    if(platform=="Switch" && (e==".nsp" || e==".xci")) return e.substr(1);
    if(platform=="Xbox 360" && (e==".iso" || (e==".xex" && lowercase(basename(path))=="default.xex"))) return e.substr(1);
    if(platform=="PS1" && (e==".cue" || e==".chd")) return e.substr(1);
    if(platform=="PSP" && (e==".iso" || e==".cso" || e==".chd" || e==".pbp"))return e.substr(1);
    if(platform=="PS3" && (e==".iso" || e==".pkg"))return e.substr(1);
    if(platform=="Xbox" && (e==".xbe" || e==".iso"))return e.substr(1);
    if(platform=="Dreamcast" && (e==".gdi" || e==".cdi" || e==".chd"))return e.substr(1);
    if(platform=="GameCube" && (e==".iso" || e==".gcm" || e==".rvz"))return e.substr(1);
    if(platform=="Wii" && (e==".iso" || e==".wbfs" || e==".rvz"))return e.substr(1);
    return "";
}
bool is_file(const std::string &p) {struct stat st{};return lstat(p.c_str(),&st)==0 && S_ISREG(st.st_mode);}
// Never point the PNG renderer at a JPG: JPEG is not supported by the native decoder.
std::string local_cover(const std::string &game_path, bool folder_game, const std::string &title_id, const std::string &stable_id) {
    const auto slash=game_path.find_last_of('/');
    const std::string folder=folder_game?game_path:(slash==std::string::npos?"":game_path.substr(0,slash));
    const std::string filename=basename(game_path);
    const auto dot=filename.find_last_of('.');
    const std::string stem=folder_game?filename:(dot==std::string::npos?filename:filename.substr(0,dot));
    // User-provided cover art overrides packaged defaults, without touching games.
    const char *directories[]={"/download0/hui/ulg_v04/covers/","/app0/assets/covers/"};
    for (const char *directory:directories) {
        for(const auto &key:{stable_id,title_id,stem}) {
            if(key.empty() || key.find('/')!=std::string::npos || key.find('\\')!=std::string::npos)continue;
            const std::string path=std::string(directory)+key+".png";
            if(is_file(path))return path;
        }
    }
    // Actual cover/box-art images take precedence over square app icons.
    const char *names[]={"cover.png","Cover.png","folder.png","Folder.png","poster.png","boxart.png", "front.png"};
    for(const char *name:names){const std::string path=folder+"/"+name;if(is_file(path))return path;}
    if(!folder_game){const std::string path=folder+"/"+stem+".png";if(is_file(path))return path;}
    // Installed PS4/PS5 app icons often live in /user/appmeta, not in the
    // directory scanned by BGH. Never replace a local game cover with an icon.
    const std::string id=lowercase(title_id);
    if(id.size()==9 && (id.compare(0,4,"cusa")==0 || id.compare(0,4,"ppsa")==0) &&
       std::all_of(id.begin()+4,id.end(),[](char c){return c>='0'&&c<='9';})){
        const std::string icon="/user/appmeta/"+title_id+"/icon0.png";
        if(is_file(icon))return icon;
    }
    // PS3 disc-folder games expose their original ICON0.PNG inside PS3_GAME.
    if(folder_game){
        const std::string ps3_icon=folder+"/PS3_GAME/ICON0.PNG";
        if(is_file(ps3_icon))return ps3_icon;
    }
    // Fallback is the game's existing icon, not a new synthesized cover.
    for(const char *name:{"sce_sys/icon0.png","sce_sys/ICON0.PNG","icon0.png","icon.png","PS4_GAME/ICON0.PNG"}){
        const std::string path=folder+"/"+name;if(is_file(path))return path;
    }
    return {};
}
using NativeMeta = NativeMetadata;
// Conservative string extraction for common param.json fields; accepts no more than 256 KiB.
std::string small_file(const std::string &path){
    FILE *f=std::fopen(path.c_str(),"rb");if(!f)return {};
    if(std::fseek(f,0,SEEK_END)!=0){std::fclose(f);return {};}
    const long size=std::ftell(f);
    if(size<0 || size>262144 || std::fseek(f,0,SEEK_SET)!=0){std::fclose(f);return {};}
    std::string bytes(static_cast<std::size_t>(size),'\0');
    const bool read=bytes.empty() || std::fread(bytes.data(),1,bytes.size(),f)==bytes.size();
    std::fclose(f);return read?bytes:std::string{};
}
std::string json_field(const std::string &s,const std::string &name){
    const std::string key="\""+name+"\"";
    const std::size_t found=s.find(key);
    if(found==std::string::npos)return {};
    auto colon=s.find(':',found+key.size());
    if(colon==std::string::npos)return {};
    auto begin=s.find('\"',colon+1);
    if(begin==std::string::npos)return {};
    std::string result;
    for(std::size_t i=begin+1;i<s.size() && result.size()<300;++i){
        const char ch=s[i];
        if(ch=='\"')return result;
        if(ch=='\\' && i+1<s.size()){
            const char escaped=s[++i];
            if(escaped=='\"'||escaped=='\\'||escaped=='/')result+=escaped;
            else if(escaped=='n'||escaped=='r'||escaped=='t')result+=' ';
            else return {};
        }else result+=ch;
    }
    return {};
}
std::uint16_t le16(const unsigned char *p){return static_cast<std::uint16_t>(p[0]|(p[1]<<8));}
std::uint32_t le32(const unsigned char *p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
NativeMeta parse_sfo_bytes(const std::string &bytes){
    if(bytes.size()<20 || bytes.compare(0,4,"\0PSF",4)!=0)return {};
    const auto *b=reinterpret_cast<const unsigned char *>(bytes.data());
    const std::uint32_t keys=le32(b+8),values=le32(b+12),count=le32(b+16);
    if(count>256 || 20+count*16>bytes.size() || keys>=bytes.size() || values>=bytes.size())return {};
    NativeMeta meta;
    for(std::uint32_t i=0;i<count;++i){
        const auto *entry=b+20+i*16;
        const std::uint32_t key_at=keys+le16(entry);
        const std::uint32_t length=le32(entry+4), value_at=values+le32(entry+12);
        if(key_at>=bytes.size()||value_at>=bytes.size()||length>bytes.size()-value_at||length>1024)continue;
        const std::size_t remain=bytes.size()-key_at;
        const char *key=reinterpret_cast<const char *>(b+key_at);
        if(std::memchr(key,0,remain)==nullptr)continue;
        const char *value=reinterpret_cast<const char *>(b+value_at);
        const auto *nul=static_cast<const char *>(std::memchr(value,0,length));
        const std::string result(value,nul?static_cast<std::size_t>(nul-value):length);
        if(std::strcmp(key,"TITLE")==0)meta.title=result;
        if(std::strcmp(key,"TITLE_ID")==0)meta.title_id=result;
    }
    return meta;
}
NativeMeta native_meta(const std::string &folder){
    const auto json=small_file(folder+"/sce_sys/param.json");
    NativeMeta result{json_field(json,"titleName"),json_field(json,"titleId")};
    if(result.title.empty() || result.title_id.empty()){
        const auto sfo=parse_sfo_bytes(small_file(folder+"/sce_sys/param.sfo"));
        if(result.title.empty())result.title=sfo.title;
        if(result.title_id.empty())result.title_id=sfo.title_id;
    }
    if(result.title.empty() || result.title_id.empty()){
        const auto sfo=parse_sfo_bytes(small_file(folder+"/PS4_GAME/PARAM.SFO"));
        if(result.title.empty())result.title=sfo.title;
        if(result.title_id.empty())result.title_id=sfo.title_id;
    }
    return result;
}
bool hidden_folder(const std::string &path){
    const std::string name=lowercase(basename(path));
    return name=="$recycle.bin" || name=="system volume information" ||
           name==".trash" || name=="trash" || name=="lost+found";
}
bool valid_cusa_name(const std::string &name){
    if(name.size()!=9 || lowercase(name.substr(0,4))!="cusa")return false;
    for(std::size_t i=4;i<9;++i)if(name[i]<'0'||name[i]>'9')return false;
    return true;
}
// A PS4 installed PKG must have a strict CUSA directory and app.pkg.
// Arbitrary .pkg download files are not treated as installed games.
bool ps4_pkg_candidate(const std::string &folder,const std::string &root_platform){
    return (root_platform=="PS4" || root_platform=="PS5/PS4") &&
           valid_cusa_name(basename(folder)) && is_file(folder+"/app.pkg");
}
bool native_candidate(const std::string &folder){
    return is_file(folder+"/sce_sys/param.json") || is_file(folder+"/sce_sys/param.sfo") ||
        is_file(folder+"/PS4_GAME/PARAM.SFO") || is_file(folder+"/eboot.bin");
}
std::string guess_id(const std::string &folder) {
    // Real game dumps may have a descriptive prefix, e.g.
    // 'Pistol.Whip-PPSA12514-EUR-Game'. Accept complete embedded IDs too.
    const std::string b=basename(folder),low=lowercase(b);
    for(std::size_t pos=0;pos+9<=low.size();++pos){
        if(low.compare(pos,4,"ppsa")!=0 && low.compare(pos,4,"cusa")!=0)continue;
        if(pos>0 && std::isalnum(static_cast<unsigned char>(b[pos-1])))continue;
        bool ok=true;
        for(std::size_t j=pos+4;j<pos+9;++j)if(b[j]<'0'||b[j]>'9')ok=false;
        if(pos+9<b.size() && std::isdigit(static_cast<unsigned char>(b[pos+9])))ok=false;
        if(ok){std::string id=b.substr(pos,9);
            for(char &c:id)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
            return id;
        }
    }
    return {};
}
std::string uid(const std::string &platform, const std::string &path) {
    // Stable FNV-1a 64-bit over platform, NUL delimiter, then original path.
    unsigned long long h=14695981039346656037ull;
    for (unsigned char c:platform){h^=c;h*=1099511628211ull;}
    h^=0;h*=1099511628211ull;
    for (unsigned char c:path){h^=c;h*=1099511628211ull;}
    const char *digits="0123456789abcdef"; std::string out(16,'0');
    for(int i=15;i>=0;--i){out[static_cast<std::size_t>(i)]=digits[h&15];h>>=4;}
    return out;
}
bool stfs_file(const std::string &path) {
    FILE *f=std::fopen(path.c_str(),"rb");if(!f)return false;
    char b[4]{};auto n=std::fread(b,1,4,f);std::fclose(f);
    return n==4 && (std::memcmp(b,"CON ",4)==0 || std::memcmp(b,"LIVE",4)==0 || std::memcmp(b,"PIRS",4)==0);
}
void append_game(Catalogue &out, std::set<std::pair<std::string,std::string>> &seen,
                 std::string platform,std::string path,std::string title,std::string format,
                 std::size_t maximum, const std::string &metadata_id = {}) {
    if(out.games.size()>=maximum){out.truncated=true;return;}
    if(!seen.insert({platform,path}).second)return;
    Game item;
    item.id=uid(platform,path);item.title=std::move(title);item.platform=std::move(platform);
    item.path=std::move(path);item.format=std::move(format);item.title_id=metadata_id.empty()?guess_id(item.path):metadata_id;
    item.cover_path=local_cover(item.path,item.format=="installationsordner" || item.format=="ps3_ordner",item.title_id,item.id);
    out.games.push_back(std::move(item));
}
void add_failure(Catalogue &out,const std::string &path,const std::string &operation,int code,bool root){
    ++out.failed_operations;
    if(root)++out.failed_roots;
    // Limit diagnostic strings, but count all failures separately.
    if(out.errors.size()<256){
        out.errors.push_back(operation+" (errno="+std::to_string(code)+") "+path);
    }
}
void walk(const Root &root,const std::string &path,int level,int max_depth,std::size_t max_games,
          Catalogue &out,std::set<std::pair<std::string,std::string>> &seen) {
    if(out.truncated)return;
    errno=0;
    DIR *d=opendir(path.c_str());
    if(!d){const int err=errno;add_failure(out,path,"opendir",err,false);return;}
    std::vector<Child> children;
    errno=0;
    for(;;) {
        struct dirent *item=readdir(d);
        if(!item)break;
        std::string name=item->d_name;
        if(name=="." || name==".." || (!name.empty() && name[0]=='.')){errno=0;continue;}
        if(name.size()>255 || path.size()+name.size()+1>4096){errno=0;continue;}
        std::string child=path+(path.back()=='/'?"":"/")+name;
        struct stat st{};
        if(lstat(child.c_str(),&st)!=0){const int err=errno;add_failure(out,child,"lstat",err,false);errno=0;continue;}
        if((S_ISREG(st.st_mode) || S_ISDIR(st.st_mode)) && !hidden_folder(child))
            children.push_back({std::move(name),std::move(child),S_ISDIR(st.st_mode)});
        errno=0;
    }
    const int read_error=errno;
    if(read_error!=0)add_failure(out,path,"readdir",read_error,false);
    closedir(d);
    std::sort(children.begin(),children.end(),[](const Child &a,const Child &b){return a.name<b.name;});
    bool native = (root.platform=="PS5/PS4" || root.platform=="PS5" || root.platform=="PS4" || root.platform=="Homebrew");
    for(const auto &c:children){
        if (out.truncated) break;
        if(native) {
            // Read-only recognition of unmounted PS5 container files. Never
            // pass these file paths to registered-title launch/mount methods.
            if(!c.folder && root.platform!="PS4") {
                const std::string fmt=format_for(root.platform,c.path);
                if(!fmt.empty()){
                    const auto id=guess_id(c.path);
                    const auto appmeta_title=ps5_appmeta_title_local(id);
                    const auto title=appmeta_title.empty()
                        ?ps5_container_fallback_title(title_from(c.name),id):appmeta_title;
                    append_game(out,seen,"PS5",c.path,title,fmt,max_games,id);
                }
                continue;
            }
            if(c.folder && (native_candidate(c.path) || ps4_pkg_candidate(c.path,root.platform))) {
                const auto meta=native_meta(c.path);
                const bool has_json=is_file(c.path+"/sce_sys/param.json");
                const bool has_sfo=is_file(c.path+"/sce_sys/param.sfo") || is_file(c.path+"/PS4_GAME/PARAM.SFO");
                std::string platform=classify_native_game(c.path,meta,has_json,has_sfo);
                // The path is a scan root, never evidence that a PPSA title is a PS4 game.
                // For dedicated roots only accept the matching native platform.
                if((root.platform=="PS4" && platform=="PS5") ||
                   (root.platform=="PS5" && platform=="PS4"))continue;
                std::string title=meta.title.empty()?title_from(c.name):meta.title;
                if(meta.title.empty() && !guess_id(c.path).empty()){
                    const auto id=meta.title_id.empty()?guess_id(c.path):meta.title_id;
                    const auto appmeta_title=platform=="PS5"?ps5_appmeta_title_local(id):std::string{};
                    title=appmeta_title.empty()?"Unbekanntes Spiel ("+guess_id(c.path)+")":appmeta_title;
                }
                if(platform=="Homebrew"){
                    const std::string id=meta.title_id.empty()?guess_id(c.path):meta.title_id;
                    if(id=="PPSA99203")title="PS5SX2";
                    else if(id=="PPSA99008")title="ProsperoEden";
                    else if(id=="PPSA50011")title="PS5X360";
                    else if(id=="PPSA99079")title="BGH - BawertL's Game Hub";
                }
                append_game(out,seen,platform,c.path,title,"installationsordner",max_games,meta.title_id);
            }else if(c.folder && level<max_depth && !(root.platform=="PS4" && root.path=="/user/app" && level==0))
                walk(root,c.path,level+1,max_depth,max_games,out,seen);
            continue;
        }
        // PS3 disc dumps are *folders*, not .iso/.pkg files. Recognize the
        // exact RPCS3 layout <game>/PS3_GAME/PARAM.SFO read-only, and stop at
        // the game folder so PS3_UPDATE and USRDIR never become fake games.
        if(root.platform=="PS3" && c.folder){
            const auto sfo=small_file(c.path+"/PS3_GAME/PARAM.SFO");
            const auto meta=parse_sfo_bytes(sfo);
            if(!meta.title.empty() || !meta.title_id.empty()){
                append_game(out,seen,"PS3",c.path,
                    meta.title.empty()?title_from(c.name):meta.title,
                    "ps3_ordner",max_games,meta.title_id);
                continue;
            }
        }
        if(c.folder){if(level<max_depth)walk(root,c.path,level+1,max_depth,max_games,out,seen);continue;}
        const std::string fmt=format_for(root.platform,c.name);
        if(!fmt.empty()) {
            const std::string title=(root.platform=="Xbox 360" && lowercase(c.name)=="default.xex") ? title_from(basename(path)): title_from(c.name);
            append_game(out,seen,root.platform,c.path,title,fmt,max_games);
        }else if(root.platform=="Xbox 360" && extension(c.name).empty() && stfs_file(c.path)) {
            append_game(out,seen,root.platform,c.path,title_from(path==root.path?c.name:basename(path)),"STFS",max_games);
        }
    }
}

std::string escaped(const std::string &v){
    static const char *hex="0123456789abcdef";std::string out="\"";
    for(unsigned char c:v){if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}else if(c=='\n')out+="\\n";
        else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";
        else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=static_cast<char>(c);
    }return out+'"';
}
} // namespace

// Resolve titles from read-only PS5 app metadata, keeping the file-based
// game ID and PPSA title ID unchanged for covers and start dispatch.
std::string ps5_appmeta_title_from_json(const std::string &bytes){
    if(bytes.empty() || bytes.size()>262144)return {};
    const auto localized=bytes.find("\"localizedParameters\"");
    if(localized!=std::string::npos){
        for(const char *locale:{"de-DE","en-US","en-GB"}){
            const auto locale_pos=bytes.find("\""+std::string(locale)+"\"",localized);
            if(locale_pos==std::string::npos)continue;
            const auto start=bytes.find('{',locale_pos);
            if(start==std::string::npos)continue;
            const auto end=bytes.find('}',start);
            if(end==std::string::npos || end-start>4096)continue;
            const auto title=json_field(bytes.substr(start,end-start+1),"titleName");
            if(!title.empty())return title;
        }
    }
    return json_field(bytes,"titleName");
}
std::string ps5_appmeta_title_local(const std::string &id){
    if(!valid_native_id(id,"PPSA"))return {};
    return ps5_appmeta_title_from_json(small_file("/user/appmeta/"+id+"/param.json"));
}
std::string ps5_container_fallback_title(const std::string &title,const std::string &id){
    if(!valid_native_id(id,"PPSA"))return title;
    std::string cleaned=title;
    const std::string lowered=lowercase(cleaned);
    const std::string needle=lowercase(id);
    const auto pos=lowered.find(needle);
    if(pos==std::string::npos)return title;
    // Only strip complete PPSA tokens; never damage an unrelated title word.
    const auto token_char=[](char c){return std::isalnum(static_cast<unsigned char>(c))!=0;};
    if((pos>0 && token_char(cleaned[pos-1])) ||
       (pos+id.size()<cleaned.size() && token_char(cleaned[pos+id.size()])))return title;
    cleaned.erase(pos,id.size());
    auto trim=[](std::string &s){
        while(!s.empty() && (std::isspace(static_cast<unsigned char>(s.front())) ||
              s.front()=='_' || s.front()=='-' || s.front()==':'))s.erase(s.begin());
        while(!s.empty() && (std::isspace(static_cast<unsigned char>(s.back())) ||
              s.back()=='_' || s.back()=='-' || s.back()==':'))s.pop_back();
    };
    trim(cleaned);
    cleaned=collapse_title_spaces(cleaned);
    // An all-ID filename has no confirmed title; preserve it for diagnostics.
    return cleaned.empty()?title:cleaned;
}

NativeMetadata native_metadata_from_bytes(const std::string &json,const std::string &sfo){
    NativeMetadata result{json_field(json,"titleName"),json_field(json,"titleId")};
    const auto sf=parse_sfo_bytes(sfo);
    if(result.title.empty())result.title=sf.title;
    if(result.title_id.empty())result.title_id=sf.title_id;
    return result;
}
namespace {
bool valid_native_id(const std::string &id,const std::string &prefix){
    if(id.size()!=9 || lowercase(id.substr(0,4))!=lowercase(prefix))return false;
    for(std::size_t i=4;i<9;++i)if(id[i]<'0'||id[i]>'9')return false;
    return true;
}
bool application_name(const std::string &name){
    const auto low=lowercase(name);
    for(const char *word:{"app dumper","ps5-app-dumper","ariang","blackbox",
        "etahen","orbit store","shadowmount","payload","itemzflow",
        "package manager","ps5sx2","prosperoeden","ps5x360", "bawertl's game hub"}){
        if(low.find(word)!=std::string::npos)return true;
    }
    return false;
}
bool application_id(const std::string &id){
    static const std::set<std::string> known={
        "APDU00001","ARIA26800","BLKB00001","ETHN13600","FAKE10101",
        "FMGR88888","ORBT00001","PKGM00001","PLDM00001","WKAL00001",
        "PPSA99079","PPSA99177","PPSA99178"
    };
    return known.count(id)>0;
}
}
bool is_emulator_title_id(const std::string &id){
    return id=="PPSA99203" || id=="PPSA99008" || id=="PPSA50011" || id=="PPSA99303" || id=="PPSA99186";
}
std::string classify_native_game(const std::string &folder,const NativeMetadata &meta,bool json_present,bool sfo_present){
    const auto id=meta.title_id.empty()?guess_id(folder):meta.title_id;
    const auto dir=basename(folder);
    if(is_emulator_title_id(id) || is_emulator_title_id(guess_id(folder)))return "Homebrew";
    if(application_id(id)||application_id(guess_id(folder)) ||
       application_name(meta.title)||application_name(dir))return "Homebrew";
    if(valid_native_id(id,"CUSA"))return "PS4";
    if(valid_native_id(id,"PPSA"))return "PS5";
    // Unknown native-looking apps are not games. Preserve the diagnostic
    // classification (Unbekannt); the presentation filter omits them.
    (void)json_present;(void)sfo_present;
    return "Unbekannt";
}
std::vector<Root> load_roots(const std::string &config_path) {
    // Only known candidate paths; file parsing never modifies them.
    const Root defaults[]={
        {"PS5","/data/homebrew"}, {"PS5","/mnt/usb0"},
        {"PS4","/data/homebrew"}, {"PS4","/mnt/usb0"},
        {"PS5/PS4","/data/etaHEN/games"},
        {"PS2","/data/PCSX2/games"}, {"PS2","/mnt/usb1/PS5SX2/games"},
        {"PS2","/mnt/usb0/PS5SX2/games"}, {"Switch","/data/prosperoeden/roms"},
        {"Switch","/mnt/usb0/eden/roms"}, {"Switch","/mnt/usb1/eden/roms"},
        {"Xbox 360","/data/xbox360"},{"Xbox 360","/mnt/usb1/xbox360"},
        {"Xbox 360","/mnt/usb0/xbox360"}, {"Xbox 360","/data/homebrew/PPSA50011/assets/roms"}
    };
    FILE *f=std::fopen(config_path.c_str(),"rb");
    if(!f)return {std::begin(defaults),std::end(defaults)};
    std::vector<Root> roots;
    char line[8192];
    while(std::fgets(line,sizeof(line),f)){
        const std::size_t length=std::strlen(line);
        if(length==0 || length>4098 || line[0]=='#')continue;
        if(line[length-1]!='\n' && !std::feof(f)){
            // Oversized line; discard the remainder rather than parsing fragments.
            int c=0;while((c=std::fgetc(f))!=EOF && c!='\n'){}
            continue;
        }
        std::string entry(line);
        while(!entry.empty() && (entry.back()=='\n'||entry.back()=='\r'))entry.pop_back();
        const auto split=entry.find('|');if(split==std::string::npos)continue;
        Root r{entry.substr(0,split),entry.substr(split+1)};
        if(r.platform!="PS5/PS4"&&r.platform!="PS5"&&r.platform!="PS4"&&r.platform!="Homebrew"&&r.platform!="PS2"&&r.platform!="Switch"&&r.platform!="Xbox 360"&&r.platform!="PS1"&&r.platform!="PSP"&&r.platform!="PS3"&&r.platform!="Xbox"&&
           r.platform!="Dreamcast"&&r.platform!="GameCube"&&r.platform!="Wii")continue;
        if(r.path.empty() || r.path[0]!='/' || r.path=="/" || r.path.find("/../")!=std::string::npos || ends(r.path,"/..") || r.path.size()>4096)continue;
        roots.push_back(std::move(r));
        if(roots.size()==128)break;
    }
    std::fclose(f);
    return roots;
}
namespace {
std::string usb_mount(const std::string &path){
    for(const std::string &prefix:{std::string("/mnt/usb"),std::string("/mnt/ext")}) {
        if(path.compare(0,prefix.size(),prefix)==0){
            auto end=path.find('/',prefix.size());
            const auto component=path.substr(prefix.size(),end==std::string::npos?std::string::npos:end-prefix.size());
            bool digits=!component.empty();
            for(char c:component)if(c<'0'||c>'9')digits=false;
            if(digits)return prefix+component;
        }
    }
    return {};
}
} // namespace
RootState make_root_error(const std::string &platform,const std::string &path,
                          const std::string &operation,int code){
    RootState result{platform,path,"fehler",code,operation};
    if(code==EACCES || code==EPERM)result.status="zugriff_verweigert";
    else if(code==ENOENT || code==ENOTDIR){
        result.status="offline";
        // Only report an unmounted USB when the mountpoint itself is missing.
        const auto mount=usb_mount(path);
        if(!mount.empty()){
            struct stat st{};
            if(lstat(mount.c_str(),&st)!=0 && (errno==ENOENT || errno==ENOTDIR))
                result.status="usb_offline";
        }
    }
    return result;
}
RootState probe_root(const Root &root){
    if(!root.enabled)return {root.platform,root.path,"deaktiviert",0,""};
    if(root.path.empty() || root.path=="/" || root.path[0]!='/' || root.path.find("/../")!=std::string::npos ||
       (root.path.size()>=3 && root.path.compare(root.path.size()-3,3,"/..") == 0))
        return {root.platform,root.path,"ungueltig",EINVAL,"validation"};
    struct stat st{};
    errno=0;
    if(lstat(root.path.c_str(),&st)!=0){const int err=errno;return make_root_error(root.platform,root.path,"lstat",err);}
    if(!S_ISDIR(st.st_mode))return {root.platform,root.path,"kein_ordner",ENOTDIR,"type"};
    errno=0;
    DIR *d=opendir(root.path.c_str());
    if(!d){const int err=errno;return make_root_error(root.platform,root.path,"opendir",err);}
    // A successful opendir alone doesn't guarantee readdir is permitted.
    errno=0;
    while(readdir(d)!=nullptr)errno=0;
    const int err=errno;
    closedir(d);
    if(err!=0)return make_root_error(root.platform,root.path,"readdir",err);
    return {root.platform,root.path,"ok",0,""};
}
Catalogue scan(const std::vector<Root> &roots,int max_depth,std::size_t max_games){
    Catalogue result;std::set<std::pair<std::string,std::string>> seen;
    max_depth=std::max(0,std::min(12,max_depth));max_games=std::min(std::size_t(10000),max_games);
    for(const auto &root:roots){
        const RootState state=probe_root(root);
        result.roots.push_back(state);
        if(state.status=="deaktiviert"){++result.disabled_roots;continue;}
        if(state.status!="ok"){
            add_failure(result,root.path,state.operation,state.error_code,true);
            continue;
        }
        walk(root,root.path,0,max_depth,max_games,result,seen);
        if(result.truncated)break;
    }
    std::sort(result.games.begin(),result.games.end(),[](const Game &a,const Game &b){
        return lowercase(a.title)==lowercase(b.title)?a.platform<b.platform:lowercase(a.title)<lowercase(b.title);
    });
    return result;
}

void apply_cover_overrides(Catalogue &catalogue,const std::string &path){
    FILE *f=std::fopen(path.c_str(),"rb");if(!f)return;
    char line[8192];std::size_t lines=0;
    while(lines++<10000 && std::fgets(line,sizeof(line),f)){
        const std::size_t length=std::strlen(line);
        if(length<2 || length>=sizeof(line)-1 || line[0]=='#')continue;
        std::string entry(line);
        while(!entry.empty() && (entry.back()=='\n'||entry.back()=='\r'))entry.pop_back();
        const auto split=entry.find('|');if(split==std::string::npos)continue;
        const auto key=entry.substr(0,split),cover=entry.substr(split+1);
        if(cover.size()>4096||cover.empty()||cover[0]!='/'||!is_file(cover))continue;
        for(auto &g:catalogue.games)if(g.id==key || g.path==key)g.cover_path=cover;
    }
    std::fclose(f);
}
std::string clean_title_for_display(const std::string &title){
    // Remove only explicit known release markers. Never blindly strip all
    // parenthesized text: (Remastered), (Part 2), (Director's Cut) are titles.
    auto release_tag=[](std::string token){
        while(!token.empty() && std::isspace(static_cast<unsigned char>(token.front())))token.erase(token.begin());
        while(!token.empty() && std::isspace(static_cast<unsigned char>(token.back())))token.pop_back();
        const std::string low=lowercase(token);
        if(low=="nsw2u" || low=="nsw" || low=="usa" || low=="eur" ||
           low=="europe" || low=="japan" || low=="world" || low=="pal" ||
           low=="ntsc" || low=="t-en" || low=="t-de" || low=="t-fr" ||
           low=="en" || low=="de" || low=="fr")return true;
        if(low.size()>1 && low[0]=='v' && std::all_of(low.begin()+1,low.end(),
            [](unsigned char c){return std::isdigit(c) || c=='.';}))return true;
        if(low.size()==16 && std::all_of(low.begin(),low.end(),
            [](unsigned char c){return std::isxdigit(c);}))return true;
        if(low.size()==9 && (low.rfind("ppsa",0)==0 || low.rfind("cusa",0)==0) &&
           std::all_of(low.begin()+4,low.end(),[](unsigned char c){return std::isdigit(c);}))return true;
        return false;
    };
    std::string out;out.reserve(title.size());
    for(std::size_t i=0;i<title.size();){
        const char c=title[i];
        if(c=='(' || c=='['){
            const auto end=title.find(c=='('?')':']',i+1);
            if(end!=std::string::npos && release_tag(title.substr(i+1,end-i-1))){
                if(!out.empty() && out.back()!=' ')out.push_back(' ');
                i=end+1;continue;
            }
        }
        out.push_back(c);++i;
    }
    out=collapse_title_spaces(out);
    return out.empty()?title:out;
}

void apply_title_overrides(Catalogue &catalogue,const std::string &path){
    FILE *f=std::fopen(path.c_str(),"rb");if(!f)return;
    char line[8192];std::size_t lines=0;
    while(lines++<10000 && std::fgets(line,sizeof(line),f)){
        const std::size_t length=std::strlen(line);
        if(length<3 || length>=sizeof(line)-1 || line[0]=='#')continue;
        std::string entry(line);
        while(!entry.empty() && (entry.back()=='\n'||entry.back()=='\r'))entry.pop_back();
        const auto sep=entry.find('|');if(sep==std::string::npos)continue;
        const std::string key=entry.substr(0,sep),title=entry.substr(sep+1);
        if(key.empty()||title.empty()||title.size()>300||title.find('\r')!=std::string::npos)continue;
        for(auto &g:catalogue.games)if(g.title_id==key || g.id==key || g.path==key)g.title=title;
    }
    std::fclose(f);
    std::sort(catalogue.games.begin(),catalogue.games.end(),[](const Game &a,const Game &b){return lowercase(a.title)==lowercase(b.title)?a.platform<b.platform:lowercase(a.title)<lowercase(b.title);});
}
Catalogue merge_unavailable(Catalogue fresh, const Catalogue &previous) {
    std::set<std::string> known;
    for (const auto &g : fresh.games) known.insert(g.id);
    for (const auto &old : previous.games) {
        if (known.count(old.id)) continue;
        for (const auto &root : fresh.roots) {
            if (root.status == "ok" || root.status == "ftp_ok" || root.status == "deaktiviert") continue;
            if (old.path == root.path || (old.path.size() > root.path.size() &&
                 old.path.compare(0,root.path.size(),root.path) == 0 &&
                 old.path[root.path.size()] == '/')) {
                Game offline = old; offline.available = false;
                fresh.games.push_back(std::move(offline)); known.insert(old.id);
                break;
            }
        }
    }
    std::sort(fresh.games.begin(),fresh.games.end(),[](const Game &a,const Game &b){
        return lowercase(a.title)==lowercase(b.title)?a.platform<b.platform:lowercase(a.title)<lowercase(b.title);
    });
    return fresh;
}
namespace {
bool inside_root(const std::string &path,const std::string &root){
    return path==root || (path.size()>root.size() &&
        path.compare(0,root.size(),root)==0 && path[root.size()]=='/');
}
int preferred_score(const Game &g){
    int score=g.available?1000:0;
    if(!g.cover_path.empty() && is_file(g.cover_path))score+=600;
    if(!g.via_ftp)score+=60;
    if(g.path.compare(0,15,"/data/homebrew/")==0)score+=60;
    if(g.path.compare(0,10,"/user/app/")==0)score+=25;
    if(g.path.compare(0,18,"/mnt/usb1/homebrew")==0)score+=10;
    return score;
}
std::string canonical_key(const Game &g){
    // Never merge distinct CUSA and PPSA editions or different ROM regions.
    if((g.platform=="PS4" && valid_native_id(g.title_id,"CUSA")) ||
       (g.platform=="PS5" && valid_native_id(g.title_id,"PPSA")) ||
       (g.platform=="Homebrew" && is_emulator_title_id(g.title_id)))
        return g.platform+":"+lowercase(g.title_id);
    // ROM copies with identical display titles and file formats are one entry.
    if(g.platform=="PS2"||g.platform=="Switch"||g.platform=="Xbox 360")
        return g.platform+":"+lowercase(g.title)+":"+lowercase(g.format);
    return g.platform+":"+g.id;
}
}
void clean_catalogue(Catalogue &catalogue,const std::vector<Root> &roots){
    std::vector<Game> cleaned;
    std::map<std::string,std::size_t> by_key;
    for(auto game:catalogue.games){
        if(game.platform=="PS5" || game.platform=="PS4" || game.platform=="Homebrew"){
            const std::string guessed=guess_id(game.path);
            const std::string known_id=game.title_id.empty()?guessed:game.title_id;
            if(is_emulator_title_id(known_id)||is_emulator_title_id(guessed)){
                game.platform="Homebrew";
                if(game.title_id.empty())game.title_id=guessed;
            }else if(application_id(known_id) || application_id(guessed) ||
                     application_id(basename(game.path)) || application_name(game.title) || application_name(basename(game.path))){
                continue;
            }
            if(game.platform=="Homebrew" && !is_emulator_title_id(known_id) &&
               !is_emulator_title_id(guessed))continue;
            if(game.platform=="PS5" && !valid_native_id(known_id,"PPSA"))continue;
            if(game.platform=="PS4"){
                // PS4 catalogue entries from cached/FTP scans can have an empty
                // title_id even when guess_id(path) identified the CUSA title.
                // Prefer valid metadata; otherwise use a valid path-derived ID.
                const std::string cusa=valid_native_id(game.title_id,"CUSA")
                    ? game.title_id : guessed;
                if(!valid_native_id(cusa,"CUSA"))continue;
                game.title_id=cusa;
                for(char &ch:game.title_id)
                    if(ch>='a' && ch<='z')ch=static_cast<char>(ch-'a'+'A');
                // Normalize BEFORE canonical_key(): identical PS4 titles dedupe.
            }
        }else if(game.platform!="PS2"&&game.platform!="Switch"&&game.platform!="Xbox 360"&&
                 game.platform!="PS1"&&game.platform!="PSP"&&game.platform!="PS3"&&
                 game.platform!="Xbox"&&game.platform!="Dreamcast"&&
                 game.platform!="GameCube"&&game.platform!="Wii")continue;
        if(hidden_folder(game.path) || game.path.find("/$RECYCLE.BIN/")!=std::string::npos ||
           game.path.find("/System Volume Information/")!=std::string::npos)continue;
        // A disabled root cannot retain its old game via an offline cache entry.
        // Overlapping roots are considered only for the same platform.
        bool enabled_match=false,disabled_match=false;
        for(const auto &r:roots){
            if(!inside_root(game.path,r.path))continue;
            const bool compatible=r.platform==game.platform ||
                (r.platform=="PS5/PS4" && (game.platform=="PS5"||game.platform=="PS4")) ||
                (game.platform=="Homebrew" && (r.platform=="PS5"||r.platform=="PS5/PS4"));
            if(!compatible)continue;
            if(r.enabled)enabled_match=true;else disabled_match=true;
        }
        if(!enabled_match && disabled_match)continue;
        const auto key=canonical_key(game);
        auto found=by_key.find(key);
        if(found==by_key.end()){
            by_key.emplace(key,cleaned.size());cleaned.push_back(std::move(game));
        }else{
            auto &existing=cleaned[found->second];
            if(preferred_score(game)>preferred_score(existing)){
                if(game.cover_path.empty() && !existing.cover_path.empty() && is_file(existing.cover_path))
                    game.cover_path=existing.cover_path;
                existing=std::move(game);
            }else if(existing.cover_path.empty() && !game.cover_path.empty() && is_file(game.cover_path)){
                existing.cover_path=game.cover_path;
            }
        }
    }
    std::sort(cleaned.begin(),cleaned.end(),[](const Game &a,const Game &b){
        return lowercase(a.title)==lowercase(b.title)?a.platform<b.platform:lowercase(a.title)<lowercase(b.title);
    });
    catalogue.games=std::move(cleaned);
}
bool write_json(const Catalogue &catalogue,const std::string &file){
    FILE *f=std::fopen(file.c_str(),"wb");if(!f)return false;
    auto put=[&](const std::string &text){return std::fwrite(text.data(),1,text.size(),f)==text.size();};
    bool ok=put("{\"schema\":4,\"scan_source\":\"native_posix_read_only\",\"games\":[");
    for(std::size_t i=0;i<catalogue.games.size() && ok;++i){
        const auto &g=catalogue.games[i];
        ok=put(std::string(i?",{":"{")+"\"id\":"+escaped(g.id)+",\"title\":"+escaped(g.title)
             +",\"platform\":"+escaped(g.platform)+",\"path\":"+escaped(g.path)
             +",\"format\":"+escaped(g.format)+",\"title_id\":"+escaped(g.title_id)
             +",\"cover_path\":"+escaped(g.cover_path)+",\"available\":"+(g.available?"true":"false")
             +",\"via_ftp\":"+(g.via_ftp?"true":"false")
             +",\"launch_status\":\"nicht_verifiziert\"}");
    }
    ok=ok&&put("],\"unavailable_paths\":[");bool first=true;
    for(const auto &r:catalogue.roots)if(r.status!="ok" && ok){
        ok=put(std::string(first?"{":",{")+"\"platform\":"+escaped(r.platform)+",\"path\":"+escaped(r.path)
             +",\"status\":"+escaped(r.status)+",\"errno\":"+std::to_string(r.error_code)+",\"operation\":"+escaped(r.operation)+"}");first=false;
    }
    ok=ok&&put("],\"errors\":[");
    for(std::size_t i=0;i<catalogue.errors.size() && ok;++i)ok=put(std::string(i?",":"")+escaped(catalogue.errors[i]));
    ok=ok&&put(std::string("],\"failed_operations\":")+std::to_string(catalogue.failed_operations)+
        ",\"failed_roots\":"+std::to_string(catalogue.failed_roots)+
        ",\"disabled_roots\":"+std::to_string(catalogue.disabled_roots)+
        ",\"truncated\":"+(catalogue.truncated?"true":"false")+"}\n");
    if(std::fclose(f)!=0)ok=false;
    return ok;
}
} // namespace ulg
