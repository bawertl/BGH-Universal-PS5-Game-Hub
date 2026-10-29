// BGH V1.1.0 - user-configurable platform registry; NOT an emulator backend.
// Independently checksummed and atomic. Never changes ULGP preferences.bin.
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace ulg {
struct PlatformEntry {
    std::string id, display_name, short_label, icon_path, title_id, rom_folder;
    std::vector<std::string> extensions;
    std::vector<std::string> rom_folders; // multiple local/USB paths per emulator
    std::string app_path; // optional install folder, NOT a Windows executable
    std::string detected_title_id, detected_name, launch_protocol="auto";
    bool is_emulator=false, scan_supported=false, launch_supported=false, enabled=false;
    int sort_order=0;
};
inline std::string active_emulator_title_id(const PlatformEntry &p){
    return p.app_path.empty()?p.title_id:p.detected_title_id;
}
inline bool valid_launch_protocol(const std::string &v){
    return v=="auto" || v=="menu_only" || v=="eden_rom_argv" ||
           v=="xbox_path_argv" || v=="rpcs3_boot_app0" || v=="ps2_disc_launch";
}
struct PlatformRegistry {
    std::vector<PlatformEntry> entries;
    std::uint32_t revision=0;
};
inline PlatformRegistry default_platform_registry(){
    PlatformRegistry out;
    auto add=[&](const char* id,const char* label,const char* icon,bool emulator,
                 bool scan,bool launch,bool enabled,const char* title,const char* formats){
        PlatformEntry p;
        p.id=id;p.short_label=id;p.display_name=label;p.icon_path=icon;
        p.is_emulator=emulator;p.scan_supported=scan;p.launch_supported=launch;
        p.enabled=enabled;p.title_id=title;p.sort_order=static_cast<int>(out.entries.size())*10;
        const std::string f=formats;
        for(std::size_t i=0;i<f.size();){const auto end=f.find(',',i);
            p.extensions.push_back(f.substr(i,end==std::string::npos?std::string::npos:end-i));
            if(end==std::string::npos)break;
            i=end+1;
        }
        if(p.extensions.size()==1 && p.extensions[0].empty())p.extensions.clear();
        out.entries.push_back(std::move(p));
    };
    add("PS5","PlayStation 5","ps5.png",false,true,true,true,"","PPSA");
    add("PS4","PlayStation 4","ps4.png",false,true,true,true,"","CUSA");
    add("PS2","PlayStation 2","ps2.png",true,true,true,true,"PPSA99203",".iso,.chd,.cso,.zso");
    add("Switch","Nintendo Switch","switch.png",true,true,true,true,"PPSA99008",".nsp,.xci");
    add("Xbox 360","Xbox 360","xbox360.png",true,true,true,true,"PPSA50011",".iso,.xex,STFS");
    add("PS1","PlayStation 1","ps1.png",true,true,false,false,"",".cue,.chd");
    add("PSP","PlayStation Portable","psp.png",true,true,false,false,"",".iso,.cso,.chd,.pbp");
    add("PS3","PlayStation 3","ps3.png",true,true,false,false,"",".iso,.pkg");
    add("Xbox","Original Xbox","xbox.png",true,true,false,false,"",".xbe,.iso");
    add("Dreamcast","Sega Dreamcast","dreamcast.png",true,true,false,false,"",".gdi,.cdi,.chd");
    add("GameCube","Nintendo GameCube","gamecube.png",true,true,false,false,"",".iso,.gcm,.rvz");
    add("Wii","Nintendo Wii","wii.png",true,true,false,false,"",".iso,.wbfs,.rvz");
    return out;
}
inline bool safe_platform_path(const std::string &value){
    if(value.empty())return true;
    if(value.size()<2 || value.size()>4095 || value[0]!='/' || value.back()=='/' ||
       value.find("//")!=std::string::npos || value.find("/../")!=std::string::npos ||
       value.find("/./")!=std::string::npos || value.compare(value.size()-2,2,"/.")==0 ||
       (value.size()>=3 && value.compare(value.size()-3,3,"/..")==0))return false;
    return value.find_first_of("\r\n|;")==std::string::npos &&
           value.find('\0')==std::string::npos; // reject delimiters
}
inline bool sidebar_profile_visible(const PlatformEntry &p){
    // User controls visibility explicitly, even when no scanner is installed.
    return p.enabled;
}
inline std::vector<std::size_t> platform_order(const PlatformRegistry &registry){
    std::vector<std::size_t> result;
    for(std::size_t i=0;i<registry.entries.size();++i)result.push_back(i);
    std::stable_sort(result.begin(),result.end(),[&](std::size_t a,std::size_t b){
        return registry.entries[a].sort_order<registry.entries[b].sort_order;
    });
    return result;
}
inline bool move_platform(PlatformRegistry &registry,const std::string &id,int direction){
    if(direction!=1 && direction!=-1)return false;
    const auto order=platform_order(registry);
    for(std::size_t i=0;i<order.size();++i){
        if(registry.entries[order[i]].id!=id)continue;
        const int next=static_cast<int>(i)+direction;
        if(next<0 || next>=static_cast<int>(order.size()))return false;
        // Keep order as sequential values, so repeated swaps are deterministic.
        std::vector<std::size_t> reordered=order;
        std::swap(reordered[i],reordered[static_cast<std::size_t>(next)]);
        for(std::size_t n=0;n<reordered.size();++n)
            registry.entries[reordered[n]].sort_order=static_cast<int>(n)*10;
        return true;
    }
    return false;
}
inline std::uint32_t platform_checksum(const std::string &data){
    std::uint32_t h=2166136261u;
    for(unsigned char c:data){h^=c;h*=16777619u;}
    return h;
}
// Version 2 retains the original single folder for migration, while persisting
// up to sixteen paths and an optional emulator installation folder. Scanning
// remains managed by the pre-existing user_paths.conf root manager.
inline bool valid_platform_entry_paths(const PlatformEntry &p){
    if(!safe_platform_path(p.rom_folder) || !safe_platform_path(p.app_path) ||
       p.rom_folders.size()>16 || !valid_launch_protocol(p.launch_protocol) ||
       p.detected_name.size()>180 || p.detected_name.find_first_of("\r\n|;")!=std::string::npos ||
       (!p.detected_title_id.empty() && (p.detected_title_id.size()!=9 ||
       p.detected_title_id.compare(0,4,"PPSA")!=0 ||
       p.detected_title_id.find_first_not_of("0123456789",4)!=std::string::npos)))return false;
    std::vector<std::string> seen;
    for(const auto &folder:p.rom_folders){
        if(folder.empty() || !safe_platform_path(folder) ||
           std::find(seen.begin(),seen.end(),folder)!=seen.end())return false;
        seen.push_back(folder);
    }
    return true;
}
inline bool add_profile_folder(PlatformEntry &p,const std::string &path){
    if(path.empty() || !safe_platform_path(path) || p.rom_folders.size()>=16 ||
       std::find(p.rom_folders.begin(),p.rom_folders.end(),path)!=p.rom_folders.end())return false;
    p.rom_folders.push_back(path);
    p.rom_folder=p.rom_folders.front();
    return true;
}
inline bool remove_profile_folder(PlatformEntry &p,std::size_t index){
    if(index>=p.rom_folders.size())return false;
    p.rom_folders.erase(p.rom_folders.begin()+static_cast<std::ptrdiff_t>(index));
    p.rom_folder=p.rom_folders.empty()?std::string():p.rom_folders.front();
    return true;
}
inline std::string platform_payload(const PlatformRegistry &registry){
    std::string body="ULGR|3|"+std::to_string(registry.revision)+"\n";
    for(const auto &p:registry.entries){
        body+=p.id+"|"+(p.enabled?"1":"0")+"|"+std::to_string(p.sort_order)+"|"+
              p.rom_folder+"|"+p.app_path+"|";
        for(std::size_t i=0;i<p.rom_folders.size();++i){
            if(i>0)body+=';';
            body+=p.rom_folders[i];
        }
        body+="|"+p.detected_title_id+"|"+p.detected_name+"|"+p.launch_protocol+"\n";
    }
    return body;
}
inline bool write_platform_registry(const PlatformRegistry &reg,const std::string &path){
    if(path.empty() || reg.entries.size()!=default_platform_registry().entries.size())return false;
    for(const auto &p:reg.entries)if(!valid_platform_entry_paths(p))return false;
    const std::string body=platform_payload(reg);
    if(body.size()>32768)return false;
    char checksum[16];std::snprintf(checksum,sizeof(checksum),"%08X",platform_checksum(body));
    const std::string all=body+"CRC|"+checksum+"\n";
    const std::string temp=path+".tmp";
    FILE* file=std::fopen(temp.c_str(),"wb");if(!file)return false;
    bool ok=std::fwrite(all.data(),1,all.size(),file)==all.size();
    if(std::fflush(file)!=0)ok=false;
    if(std::fclose(file)!=0)ok=false;
    if(!ok || std::rename(temp.c_str(),path.c_str())!=0){std::remove(temp.c_str());return false;}
    return true;
}
inline bool read_platform_registry(const std::string &path,PlatformRegistry &out){
    FILE *file=std::fopen(path.c_str(),"rb");if(!file)return false;
    std::string content;char buffer[4096];
    for(;;){const auto count=std::fread(buffer,1,sizeof(buffer),file);
        content.append(buffer,count);if(content.size()>32768)break;
        if(count<sizeof(buffer))break;
    }
    const bool io_error=std::ferror(file)!=0;std::fclose(file);
    if(io_error || content.size()>32768 || content.empty() || content.back()!='\n')return false;
    const auto checksum_at=content.rfind("CRC|");
    if(checksum_at==std::string::npos || checksum_at+13!=content.size())return false;
    const std::string body=content.substr(0,checksum_at);
    unsigned value=0;
    if(content.substr(checksum_at+4,8).find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos ||
       std::sscanf(content.c_str()+checksum_at+4,"%8X",&value)!=1 ||
       static_cast<std::uint32_t>(value)!=platform_checksum(body))return false;
    PlatformRegistry result=default_platform_registry();
    const auto header_end=body.find('\n');
    if(header_end==std::string::npos)return false;
    const bool old_format=body.compare(0,7,"ULGR|1|")==0;
    const bool third_format=body.compare(0,7,"ULGR|3|")==0;
    if(!old_format && !third_format && body.compare(0,7,"ULGR|2|")!=0)return false;
    const std::string revision=body.substr(7,header_end-7);
    if(revision.empty() || revision.find_first_not_of("0123456789")!=std::string::npos)return false;
    const unsigned long rev=std::strtoul(revision.c_str(),nullptr,10);
    if(rev>UINT32_MAX)return false;
    result.revision=static_cast<std::uint32_t>(rev);
    std::vector<bool> seen(result.entries.size(),false);
    std::size_t pos=header_end+1;
    while(pos<body.size()){
        const auto end=body.find('\n',pos);if(end==std::string::npos)return false;
        const auto line=body.substr(pos,end-pos);pos=end+1;
        std::vector<std::string> fields;
        std::size_t cursor=0;
        while(cursor<=line.size()){
            const auto sep=line.find('|',cursor);
            fields.push_back(line.substr(cursor,sep==std::string::npos?std::string::npos:sep-cursor));
            if(sep==std::string::npos)break;
            cursor=sep+1;
        }
        if(fields.size()!=(old_format?4u:(third_format?9u:6u)))return false;
        const auto &id=fields[0],&enabled=fields[1],&ordinal=fields[2],&path_value=fields[3];
        if((enabled!="1"&&enabled!="0") || ordinal.empty() ||
           ordinal.find_first_not_of("0123456789")!=std::string::npos ||
           !safe_platform_path(path_value))return false;
        const unsigned long rank=std::strtoul(ordinal.c_str(),nullptr,10);
        if(rank>100000u)return false;
        auto it=std::find_if(result.entries.begin(),result.entries.end(),[&](const auto &e){return e.id==id;});
        if(it==result.entries.end())return false;
        const auto index=static_cast<std::size_t>(it-result.entries.begin());
        if(seen[index])return false;
        seen[index]=true;
        it->enabled=enabled=="1";it->sort_order=static_cast<int>(rank);
        it->rom_folder=path_value;
        if(!old_format){
            it->app_path=fields[4];
            if(third_format){
                it->detected_title_id=fields[6];
                it->detected_name=fields[7];
                it->launch_protocol=fields[8];
            }
            const auto &folders=fields[5];
            std::size_t cursor=0;
            while(cursor<folders.size()){
                const auto sep=folders.find(';',cursor);
                const std::string folder=folders.substr(cursor,sep==std::string::npos?
                    std::string::npos:sep-cursor);
                if(!add_profile_folder(*it,folder))return false;
                if(sep==std::string::npos)break;
                cursor=sep+1;
            }
        }else if(!path_value.empty()){
            it->rom_folder.clear();
            if(!add_profile_folder(*it,path_value))return false;
        }
        if(!valid_platform_entry_paths(*it))return false;
    }
    if(std::find(seen.begin(),seen.end(),false)!=seen.end())return false;
    out=std::move(result);return true;
}
} // namespace ulg
