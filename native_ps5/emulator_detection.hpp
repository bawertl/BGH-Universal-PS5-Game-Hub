// BGH V1.1.9 - read-only PS5 homebrew emulator identification.
// Profiles never execute ELF helpers or supply unverified ROM arguments.
// SPDX-License-Identifier: MIT
#pragma once
#include "platform_registry.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace ulg {
namespace emulator_detection {
struct Json {
    enum class Type { null_value, text, integer, list, object } type=Type::null_value;
    std::string text;
    std::vector<Json> list;
    std::map<std::string,Json> object;
    const Json &get(const std::string &key) const {
        static const Json empty{};
        auto it=object.find(key);return it==object.end()?empty:it->second;
    }
};
class Parser {
    const std::string &source_;
    std::size_t at_=0;
    void ws(){while(at_<source_.size() && (source_[at_]==' '||source_[at_]=='\t'||source_[at_]=='\r'||source_[at_]=='\n'))++at_;}
    bool consume(char c){ws();if(at_>=source_.size()||source_[at_]!=c)return false;++at_;return true;}
    bool string(std::string &result){
        if(!consume('"'))return false;
        while(at_<source_.size() && result.size()<4096){
            unsigned char c=static_cast<unsigned char>(source_[at_++]);
            if(c=='"')return true;
            if(c<32)return false;
            if(c=='\\'){
                if(at_>=source_.size())return false;
                char escape=source_[at_++];
                if(escape=='"'||escape=='\\'||escape=='/')result+=escape;
                else if(escape=='n'||escape=='r'||escape=='t')result+=' ';
                else return false; // no ambiguous Unicode escapes in executable identifiers
            }else result+=static_cast<char>(c);
        }
        return false;
    }
    bool value(Json &v,int depth){
        if(depth>8)return false;
        ws();if(at_>=source_.size())return false;
        if(source_[at_]=='"'){v.type=Json::Type::text;return string(v.text);}
        if(consume('{')){
            v.type=Json::Type::object;if(consume('}'))return true;
            for(std::size_t n=0;n<128;++n){
                std::string key;Json child;
                if(!string(key)||!consume(':')||!value(child,depth+1))return false;
                if(!v.object.emplace(std::move(key),std::move(child)).second)return false;
                if(consume('}'))return true;
                if(!consume(','))return false;
            }return false;
        }
        if(consume('[')){
            v.type=Json::Type::list;if(consume(']'))return true;
            for(std::size_t n=0;n<128;++n){Json child;
                if(!value(child,depth+1))return false;
                v.list.push_back(std::move(child));
                if(consume(']'))return true;
                if(!consume(','))return false;
            }return false;
        }
        if(source_[at_]>='0' && source_[at_]<='9'){
            v.type=Json::Type::integer;
            while(at_<source_.size() && std::isdigit(static_cast<unsigned char>(source_[at_])))
                v.text+=source_[at_++];
            return v.text.size()<12;
        }
        if(source_.compare(at_,4,"null")==0){at_+=4;return true;}
        return false;
    }
public:
    explicit Parser(const std::string &s):source_(s){}
    bool parse(Json &v){if(!value(v,0))return false;ws();return at_==source_.size();}
};
inline bool file_bytes(const std::string &path,std::string &out,std::size_t cap=65536){
    out.clear();FILE *f=std::fopen(path.c_str(),"rb");if(!f)return false;
    char buffer[2048];bool ok=true;
    for(;;){const auto n=std::fread(buffer,1,sizeof(buffer),f);
        if(out.size()+n>cap){ok=false;break;}
        out.append(buffer,n);
        if(n<sizeof(buffer)){if(std::ferror(f))ok=false;break;}
    }
    std::fclose(f);if(!ok)out.clear();return ok;
}
inline bool ordinary_file(const std::string &path){
    struct stat st{};return lstat(path.c_str(),&st)==0 && S_ISREG(st.st_mode);
}
inline bool ps5_title_id(const std::string &id){
    if(id.size()!=9 || id.compare(0,4,"PPSA")!=0)return false;
    for(std::size_t i=4;i<9;++i)if(id[i]<'0'||id[i]>'9')return false;
    return true;
}
struct Profile {
    std::string platform, id, title_id, name;
    std::vector<std::string> required_files;
    // No arbitrary executable path, command or ROM argument templates are accepted.
};
inline bool parse_profiles(const std::string &bytes,std::vector<Profile> &out){
    if(bytes.empty()||bytes.size()>65536)return false;
    Json root;if(!Parser(bytes).parse(root) || root.type!=Json::Type::object ||
       root.get("schema").text!="1" || root.get("emulators").type!=Json::Type::list)return false;
    std::vector<Profile> result;
    for(const auto &node:root.get("emulators").list){
        if(node.type!=Json::Type::object)return false;
        Profile p;
        const auto &platform=node.get("platform"),&id=node.get("id"),&title=node.get("title_id"),
                   &name=node.get("name"),&required=node.get("required_files");
        if(platform.type!=Json::Type::text||id.type!=Json::Type::text||
           title.type!=Json::Type::text||name.type!=Json::Type::text||
           required.type!=Json::Type::list)return false;
        p.platform=platform.text;p.id=id.text;p.title_id=title.text;p.name=name.text;
        if(!ps5_title_id(p.title_id)||p.id.empty()||p.id.size()>40||
           p.name.empty()||p.name.size()>80)return false;
        const auto defaults=default_platform_registry();
        auto found=std::find_if(defaults.entries.begin(),defaults.entries.end(),[&](const PlatformEntry &e){
            return e.id==p.platform && e.is_emulator;
        });
        if(found==defaults.entries.end())return false;
        for(const auto &item:required.list){
            if(item.type!=Json::Type::text || item.text.empty()||item.text.size()>80||
               item.text[0]=='/' || item.text.find("..")!=std::string::npos ||
               item.text.find('\\')!=std::string::npos)return false;
            p.required_files.push_back(item.text);
        }
        if(p.required_files.empty()||p.required_files.size()>12)return false;
        result.push_back(std::move(p));
    }
    if(result.size()>32)return false;
    out=std::move(result);return true;
}
inline std::vector<Profile> built_in_profiles(){
    return {
        {"PS2","ps5sx2","PPSA99203","PS5SX2",{"eboot.bin"}},
        {"Switch","prosperoeden","PPSA99008","ProsperoEden",{"eboot.bin"}},
        {"Xbox 360","ps5x360","PPSA50011","PS5X360",{"eboot.bin"}},
        {"PS3","rpcs3ps5","PPSA99303","RPCS3 PS5",{"eboot.bin","rpcs3ps5-launch.elf"}}
    };
}
inline std::vector<Profile> load_profiles(const std::string &bundled="/app0/assets/emulator_profiles.json",
                                          const std::string &custom="/data/bgh/emulator_profiles.json"){
    auto profiles=built_in_profiles();
    for(const auto &file:{bundled,custom}){
        std::string bytes;std::vector<Profile> parsed;
        if(!file_bytes(file,bytes) || !parse_profiles(bytes,parsed))continue;
        for(auto &p:parsed){
            auto it=std::find_if(profiles.begin(),profiles.end(),[&](const Profile &old){
                return old.platform==p.platform && old.title_id==p.title_id;
            });
            if(it!=profiles.end())*it=p;
            else if(profiles.size()<64)profiles.push_back(p);
        }
    }
    return profiles;
}
struct Detection {
    enum class Status { no_path, invalid_path, incomplete, mismatch, unidentified_app, known_emulator } status=Status::no_path;
    std::string title_id, name, profile_id;
};
inline Detection detect(const std::string &path,const std::string &platform,
                        const std::vector<Profile> &profiles){
    Detection result;
    if(path.empty())return result;
    result.status=Detection::Status::invalid_path;
    if(!safe_platform_path(path))return result;
    struct stat st{};
    if(lstat(path.c_str(),&st)!=0 || !S_ISDIR(st.st_mode))return result;
    result.status=Detection::Status::incomplete;
    if(!ordinary_file(path+"/eboot.bin"))return result;
    std::string bytes;Json meta;
    if(!file_bytes(path+"/sce_sys/param.json",bytes) || !Parser(bytes).parse(meta) ||
       meta.type!=Json::Type::object)return result;
    const auto &id=meta.get("titleId");
    if(id.type!=Json::Type::text || !ps5_title_id(id.text))return result;
    // Reject a contradicting installed-app folder ID (never silently trust it).
    const auto slash=path.find_last_of('/');
    const auto basename=path.substr(slash==std::string::npos?0:slash+1);
    if(ps5_title_id(basename) && basename!=id.text){result.status=Detection::Status::mismatch;return result;}
    result.title_id=id.text;
    const auto &localized=meta.get("localizedParameters");
    const auto &default_lang=localized.get("defaultLanguage");
    const auto &title=default_lang.get("titleName");
    if(title.type==Json::Type::text)result.name=title.text;
    if(result.name.empty() && meta.get("titleName").type==Json::Type::text)
        result.name=meta.get("titleName").text;
    if(result.name.empty())result.name="PS5-Anwendung";
    if(result.name.size()>100)result.name.resize(100);
    result.status=Detection::Status::unidentified_app;
    for(const auto &p:profiles){
        if(p.platform!=platform||p.title_id!=id.text)continue;
        bool matched=true;
        for(const auto &file:p.required_files)
            if(!ordinary_file(path+"/"+file)){matched=false;break;}
        if(!matched)continue;
        result.status=Detection::Status::known_emulator;
        result.profile_id=p.id;
        if(!p.name.empty())result.name=p.name;
        break;
    }
    return result;
}
inline std::string description(const Detection &d){
    switch(d.status){
    case Detection::Status::no_path:return "Vorlage / kein Backend";
    case Detection::Status::invalid_path:return "Installationsordner nicht gefunden";
    case Detection::Status::incomplete:return "App-Metadaten fehlen / nicht lesbar";
    case Detection::Status::mismatch:return "Titel-ID stimmt nicht mit Ordner ueberein";
    case Detection::Status::unidentified_app:return "PS5-App erkannt / Emulator-Typ offen";
    case Detection::Status::known_emulator:return d.name+" erkannt / Menue-Start ungetestet";
    }
    return "Nicht erkannt";
}
} // namespace emulator_detection
} // namespace ulg
