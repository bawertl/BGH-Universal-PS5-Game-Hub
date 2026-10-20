// SPDX-License-Identifier: MIT
#include "state_store.hpp"
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace ulg { namespace {
constexpr std::size_t kMaxFile = 16 * 1024 * 1024;
constexpr std::size_t kMaxField = 4096;
struct Writer {
    std::string bytes;
    void number(std::uint32_t n) {
        for (int i=0;i<4;++i) bytes.push_back(static_cast<char>(n >> (8*i)));
    }
    void str(const std::string &s) {
        number(static_cast<std::uint32_t>(s.size())); bytes.append(s);
    }
};
struct Reader {
    const std::string &bytes;
    std::size_t pos = 0;
    bool good = true;
    std::uint32_t number() {
        if (!good || pos>bytes.size() || bytes.size()-pos<4) {good=false;return 0;}
        std::uint32_t n=0;
        for(int i=0;i<4;++i) n|=static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[pos++]))<<(8*i);
        return n;
    }
    std::string str() {
        const std::uint32_t len=number();
        if (!good || len>kMaxField || pos>bytes.size() || len>bytes.size()-pos) {good=false;return {};}
        std::string result=bytes.substr(pos,len);pos+=len;return result;
    }
};
std::uint32_t checksum(const std::string &data) {
    std::uint32_t h=2166136261u;
    for (unsigned char c : data) {h^=c;h*=16777619u;}
    return h;
}
void storage_result(StorageStatus *result, bool ok, const char *stage, int os_error=0){
    if(!result)return;
    result->ok=ok;
    result->stage=stage;
    result->os_error=os_error;
}
bool save(const std::string &path, const std::string &magic, Writer &w,
          StorageStatus *status=nullptr) {
    storage_result(status,false,"invalid_arguments");
    if(path.empty() || path.size()>4000 || w.bytes.size()>kMaxFile-8)return false;
    std::string complete=magic+w.bytes;
    Writer tail;tail.number(checksum(complete));complete+=tail.bytes;
    const std::string temporary=path+".tmp";
    FILE *output=std::fopen(temporary.c_str(),"wb");
    if(!output){const int error=errno;storage_result(status,false,"open_tmp",error);return false;}
    bool ok=true;
    if(std::fwrite(complete.data(),1,complete.size(),output)!=complete.size()){
        const int error=errno;storage_result(status,false,"write",error);ok=false;
    }
    if(std::fflush(output)!=0){
        const int error=errno;
        if(ok)storage_result(status,false,"flush",error);
        ok=false;
    }
    if(std::fclose(output)!=0){
        const int error=errno;
        if(ok)storage_result(status,false,"close",error);
        ok=false;
    }
    if(!ok){std::remove(temporary.c_str());return false;}
    if(std::rename(temporary.c_str(),path.c_str())!=0){
        const int error=errno;
        std::remove(temporary.c_str());
        storage_result(status,false,"rename",error);return false;
    }
    storage_result(status,true,"saved");return true;
}
bool load(const std::string &path, const std::string &magic, std::string &bytes,
          StorageStatus *status=nullptr) {
    storage_result(status,false,"not_read");
    FILE *in=std::fopen(path.c_str(),"rb");
    if(!in){const int error=errno;storage_result(status,false,"open",error);return false;}
    if(std::fseek(in,0,SEEK_END)!=0){
        const int error=errno;std::fclose(in);storage_result(status,false,"seek_end",error);return false;
    }
    const long size=std::ftell(in);
    if(size<0){const int error=errno;std::fclose(in);storage_result(status,false,"tell",error);return false;}
    if(size<static_cast<long>(magic.size()+4) || size>static_cast<long>(kMaxFile)){
        std::fclose(in);storage_result(status,false,"invalid_size");return false;
    }
    if(std::fseek(in,0,SEEK_SET)!=0){
        const int error=errno;std::fclose(in);storage_result(status,false,"seek_start",error);return false;
    }
    std::string full(static_cast<std::size_t>(size),'\0');
    if(std::fread(full.data(),1,full.size(),in)!=full.size()){
        const int error=errno;std::fclose(in);storage_result(status,false,"read",error);return false;
    }
    if(std::fclose(in)!=0){
        const int error=errno;storage_result(status,false,"close",error);return false;
    }
    if(full.compare(0,magic.size(),magic)!=0){storage_result(status,false,"magic_mismatch");return false;}
    const std::size_t position=full.size()-4;
    std::uint32_t stored=0;
    for(int i=0;i<4;++i)stored|=static_cast<std::uint32_t>(static_cast<unsigned char>(full[position+i]))<<(8*i);
    full.resize(position);
    if(checksum(full)!=stored){storage_result(status,false,"checksum");return false;}
    bytes=full.substr(magic.size());storage_result(status,true,"loaded");return true;
}
} // namespace
bool write_catalogue_cache(const Catalogue &catalogue,const std::string &path){
    Writer w;w.number(static_cast<std::uint32_t>(catalogue.games.size()));
    for(const auto &g:catalogue.games){
        for(const auto *s:{&g.id,&g.title,&g.platform,&g.path,&g.format,&g.title_id,&g.cover_path}){
            if(s->size()>kMaxField)return false;
            w.str(*s);
        }
        w.number(g.available?1u:0u);
        w.number(g.via_ftp?1u:0u);
    }
    w.number(static_cast<std::uint32_t>(catalogue.roots.size()));
    for(const auto &r:catalogue.roots){
        for(const auto *field:{&r.platform,&r.path,&r.status,&r.operation}){
            if(field->size()>kMaxField)return false;
            w.str(*field);
        }
        w.number(static_cast<std::uint32_t>(r.error_code));
    }
    w.number(static_cast<std::uint32_t>(catalogue.failed_operations));
    w.number(static_cast<std::uint32_t>(catalogue.failed_roots));
    w.number(static_cast<std::uint32_t>(catalogue.disabled_roots));
    return save(path,"ULG6",w);
}
bool read_catalogue_cache(const std::string &path,Catalogue &out){
    std::string bytes;if(!load(path,"ULG6",bytes))return false;
    Reader r{bytes};Catalogue next;
    const auto count=r.number();if(count>10000)return false;
    for(std::uint32_t i=0;i<count && r.good;++i){
        Game g;g.id=r.str();g.title=r.str();g.platform=r.str();g.path=r.str();
        g.format=r.str();g.title_id=r.str();g.cover_path=r.str();g.available=r.number()==1;
        g.via_ftp=r.number()==1;
        if(g.path.empty() || g.path[0]!='/' || g.id.empty())return false;
        next.games.push_back(std::move(g));
    }
    const auto roots=r.number();if(roots>128)return false;
    for(std::uint32_t i=0;i<roots && r.good;++i){
        RootState s;s.platform=r.str();s.path=r.str();s.status=r.str();s.operation=r.str();
        s.error_code=static_cast<int>(r.number());next.roots.push_back(std::move(s));
    }
    next.failed_operations=r.number();next.failed_roots=r.number();next.disabled_roots=r.number();
    if(next.failed_roots>next.failed_operations || next.failed_operations>1000000)return false;
    if(!r.good || r.pos!=bytes.size())return false;
    out=std::move(next);return true;
}
bool write_preferences(const Preferences &prefs,const std::string &path,StorageStatus *status){
    if(prefs.favorites.size()>10000 || prefs.recent.size()>256){
        storage_result(status,false,"invalid_counts");return false;
    }
    Writer w;w.number(static_cast<std::uint32_t>(prefs.favorites.size()));
    for(const auto &id:prefs.favorites){if(id.size()>kMaxField){storage_result(status,false,"field_too_large");return false;}w.str(id);}
    w.number(static_cast<std::uint32_t>(prefs.recent.size()));
    for(const auto &id:prefs.recent){if(id.size()>kMaxField){storage_result(status,false,"field_too_large");return false;}w.str(id);}
    return save(path,"ULGP",w,status);
}
bool read_preferences(const std::string &path,Preferences &out,StorageStatus *status){
    std::string bytes;if(!load(path,"ULGP",bytes,status))return false;
    Reader r{bytes};Preferences prefs;
    const auto count=r.number();if(count>10000){storage_result(status,false,"invalid_favorite_count");return false;}
    for(std::uint32_t i=0;i<count && r.good;++i)prefs.favorites.insert(r.str());
    const auto recent=r.number();if(recent>256){storage_result(status,false,"invalid_recent_count");return false;}
    for(std::uint32_t i=0;i<recent && r.good;++i)prefs.recent.push_back(r.str());
    if(!r.good || r.pos!=bytes.size()){storage_result(status,false,"invalid_content");return false;}
    out=std::move(prefs);storage_result(status,true,"loaded");return true;
}
// Do not alter ULGP: the opt-in setting is saved independently under ULGE.
// Revision selects the most recently written replica when only one of the
// title's private storage mounts was writable during the last update.
bool write_emulator_boot_settings(const EmulatorBootSettings &settings,const std::string &path,StorageStatus *status){
    Writer w;
    w.number(1u); // file format version
    w.number(settings.revision);
    w.number(settings.direct_boot?1u:0u);
    return save(path,"ULGE",w,status);
}
bool read_emulator_boot_settings(const std::string &path,EmulatorBootSettings &out,StorageStatus *status){
    std::string bytes;
    if(!load(path,"ULGE",bytes,status))return false;
    Reader r{bytes};
    const std::uint32_t format=r.number();
    const std::uint32_t revision=r.number();
    const std::uint32_t direct=r.number();
    if(!r.good || r.pos!=bytes.size() || format!=1u || direct>1u){
        storage_result(status,false,"invalid_content");return false;
    }
    out=EmulatorBootSettings{direct!=0u,revision};
    storage_result(status,true,"loaded");
    return true;
}
// ULGL v1 is a separate revisioned/checksummed language preference; no migration.
bool write_language_settings(const LanguageSettings &settings,const std::string &path,StorageStatus *status){
    Writer w;
    w.number(1u);
    w.number(settings.revision);
    w.number(settings.english?1u:0u);
    return save(path,"ULGL",w,status);
}
bool read_language_settings(const std::string &path,LanguageSettings &out,StorageStatus *status){
    std::string bytes;
    if(!load(path,"ULGL",bytes,status))return false;
    Reader r{bytes};
    const auto version=r.number();
    const auto revision=r.number();
    const auto language=r.number();
    if(!r.good || r.pos!=bytes.size() || version!=1u || language>1u){
        storage_result(status,false,"invalid_content");return false;
    }
    out=LanguageSettings{language==1u,revision};
    storage_result(status,true,"loaded");
    return true;
}
// ULGT v1: a separate checksummed, revisioned visual-only preference.
bool write_theme_settings(const ThemeSettings &settings,const std::string &path,StorageStatus *status){
    Writer w;
    w.number(1u);
    w.number(settings.revision);
    w.number(settings.frosted_glass?1u:0u);
    return save(path,"ULGT",w,status);
}
bool read_theme_settings(const std::string &path,ThemeSettings &out,StorageStatus *status){
    std::string bytes;
    if(!load(path,"ULGT",bytes,status))return false;
    Reader r{bytes};
    const auto version=r.number();
    const auto revision=r.number();
    const auto style=r.number();
    if(!r.good || r.pos!=bytes.size() || version!=1u || style>1u){
        storage_result(status,false,"invalid_content");return false;
    }
    out=ThemeSettings{style==1u,revision};
    storage_result(status,true,"loaded");
    return true;
}
// ULGM v1: isolated, revisioned, checksummed music preference.
bool write_music_settings(const MusicSettings &settings,const std::string &path,StorageStatus *status){
    Writer w; w.number(1u); w.number(settings.revision); w.number(settings.enabled?1u:0u);
    return save(path,"ULGM",w,status);
}
bool read_music_settings(const std::string &path,MusicSettings &out,StorageStatus *status){
    std::string bytes;
    if(!load(path,"ULGM",bytes,status))return false;
    Reader r{bytes};
    const auto version=r.number(), revision=r.number(), enabled=r.number();
    if(!r.good || r.pos!=bytes.size() || version!=1u || enabled>1u){
        storage_result(status,false,"invalid_content"); return false;
    }
    out=MusicSettings{enabled==1u,revision};
    storage_result(status,true,"loaded"); return true;
}
} // namespace ulg
