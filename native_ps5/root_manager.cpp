// SPDX-License-Identifier: MIT
#include "root_manager.hpp"
#include "ftp_readonly.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <set>
#include <sys/stat.h>
#include <utility>

namespace ulg {
namespace {
bool platform_ok(const std::string &platform) {
    return platform=="PS5/PS4" || platform=="PS5" || platform=="PS4" || platform=="PS2" ||
           platform=="Switch" || platform=="Xbox 360" || platform=="PS1" || platform=="PSP" ||
           platform=="PS3" || platform=="Xbox" || platform=="Dreamcast" ||
           platform=="GameCube" || platform=="Wii";
}
bool child_path(const std::string &p,const std::string &name,std::string &out) {
    if(p.empty() || p.size()+name.size()+2>4096 || name.empty() || name=="." || name=="..")return false;
    out=p+(p=="/"?"":"/")+name;return true;
}
}
bool ensure_native_roots(std::vector<Root>& roots) {
    bool changed=false;
    for(const char* platform : {"PS5", "PS4"}) {
        bool has_any=false;
        for(const auto &r:roots)if(r.platform==platform){has_any=true;break;}
        // V0.8 only had a combined PS5/PS4 path. Add independent roots once;
        // an existing user's individual platform paths are never overwritten.
        if(has_any)continue;
        for(const char* path : {"/data/homebrew", "/mnt/usb0"}) {
            Root r{platform,path};
            if(roots.size()<128) {roots.push_back(r);changed=true;}
        }
    }
    return changed;
}
bool ensure_ps5_usb_parent_root(std::vector<Root>& roots) {
    const std::string parent="/mnt/usb1";
    bool has_enabled_child=false;
    for(const auto &r:roots){
        if(r.platform!="PS5" && r.platform!="PS5/PS4")continue;
        // The user can explicitly disable this root; never override that decision.
        if(r.path==parent)return false;
        if(r.enabled && r.path.size()>parent.size() &&
           r.path.compare(0,parent.size(),parent)==0 && r.path[parent.size()]=='/')
            has_enabled_child=true;
    }
    if(!has_enabled_child || roots.size()>=128)return false;
    roots.push_back({"PS5",parent,true});
    return true;
}
bool ensure_ps4_install_root(std::vector<Root>& roots) {
    for(const auto &r:roots)if(r.platform=="PS4" && r.path=="/user/app")return false;
    if(roots.size()>=128)return false;
    roots.push_back({"PS4","/user/app",true});
    return true;
}
bool valid_root(const Root &r) {
    if(!platform_ok(r.platform) || r.path.size()<2 || r.path.size()>4095 || r.path[0]!='/')return false;
    if(r.path.back()=='/' || r.path.find("//")!=std::string::npos || r.path.find("/./")!=std::string::npos ||
       r.path.find("/../")!=std::string::npos || r.path.substr(r.path.size()-2)=="/.")return false;
    if(r.path.size()>=3 && r.path.compare(r.path.size()-3,3,"/..") == 0)return false;
    return r.path.find_first_of("\r\n|")==std::string::npos &&
           r.platform.find_first_of("\r\n|")==std::string::npos;
}
bool save_user_roots(const std::vector<Root> &roots,const std::string &private_path) {
    if(roots.size()>128 || private_path.size()>3800 || private_path.empty())return false;
    std::set<std::pair<std::string,std::string>> seen;
    for(const auto &r:roots)if(!valid_root(r) || !seen.insert({r.platform,r.path}).second)return false;
    const std::string temp=private_path+".tmp";
    FILE *f=std::fopen(temp.c_str(),"wb");if(!f)return false;
    bool ok=std::fputs("# ULG roots version 2 - only private application storage\n",f)>=0;
    for(const auto &r:roots){
        const std::string line=std::string(r.enabled?"1|":"0|")+r.platform+"|"+r.path+"\n";
        if(ok)ok=std::fwrite(line.data(),1,line.size(),f)==line.size();
    }
    if(std::fflush(f)!=0)ok=false;
    if(std::fclose(f)!=0)ok=false;
    if(!ok){std::remove(temp.c_str());return false;}
    if(std::rename(temp.c_str(),private_path.c_str())!=0){std::remove(temp.c_str());return false;}
    return true;
}
std::vector<Root> configured_roots(const std::string &private_path,const std::string &packaged_path) {
    FILE *f=std::fopen(private_path.c_str(),"rb");if(!f)return load_roots(packaged_path);
    std::vector<Root> roots;std::set<std::pair<std::string,std::string>> seen;
    bool corrupted=false;
    char line[8192];
    while(std::fgets(line,sizeof(line),f)){
        const std::size_t n=std::strlen(line);
        if(n>=sizeof(line)-1){corrupted=true;break;}
        if(line[0]=='#'||line[0]=='\n'||line[0]=='\r')continue;
        std::string row(line);while(!row.empty()&&(row.back()=='\n'||row.back()=='\r'))row.pop_back();
        if(row.size()<5 || row[1]!='|' || (row[0]!='0'&&row[0]!='1')){corrupted=true;break;}
        const auto second=row.find('|',2);if(second==std::string::npos){corrupted=true;break;}
        Root r{row.substr(2,second-2),row.substr(second+1),row[0]=='1'};
        if(!valid_root(r) || !seen.insert({r.platform,r.path}).second || roots.size()>=128){corrupted=true;break;}
        roots.push_back(std::move(r));
    }
    if(std::ferror(f))corrupted=true;
    std::fclose(f);
    return corrupted?load_roots(packaged_path):roots;
}
BrowserResult browse_folders(const std::string &path,std::size_t max_entries){
    BrowserResult out;out.path=path;out.state=path=="/"?RootState{"Browse",path,"ok",0,""}:probe_root({"Browse",path});
    if(out.state.status!="ok"){
        // Reuse user's existing ftpsrv: no elevation and no filesystem writes.
        FtpReadonly ftp;
        if(ftp.connect_local()){
            const auto remote=ftp.list(path,std::min(max_entries,std::size_t(256)));
            if(remote.ok){
                out.state={"Browse",path,"ftp_ok",0,"FTP MLSD"};
                for(const auto &e:remote.entries){
                    if(!e.folder)continue;
                    std::string candidate;
                    if(child_path(path,e.name,candidate))out.entries.push_back({e.name,std::move(candidate)});
                }
                std::sort(out.entries.begin(),out.entries.end(),[](const BrowserEntry &a,const BrowserEntry &b){return a.name<b.name;});
            }
        }
        return out;
    }
    DIR *dir=opendir(path.c_str());
    if(!dir){out.state=make_root_error("Browse",path,"opendir",errno);return out;}
    max_entries=std::min(max_entries,std::size_t(256));
    int read_error=0;
    for(;;){
        errno=0;
        struct dirent *entry=readdir(dir);
        if(!entry){read_error=errno;break;}
        if(std::strcmp(entry->d_name,".")==0 || std::strcmp(entry->d_name,"..")==0)continue;
        const std::string name=entry->d_name;
        std::string candidate;
        if(!child_path(path,name,candidate))continue;
        struct stat st{};
        if(lstat(candidate.c_str(),&st)!=0)continue;
        if(!S_ISDIR(st.st_mode))continue;
        if(out.entries.size()<max_entries)out.entries.push_back({name,std::move(candidate)});
    }
    closedir(dir);
    if(read_error!=0){out.state=make_root_error("Browse",path,"readdir",read_error);}
    std::sort(out.entries.begin(),out.entries.end(),[](const BrowserEntry &a,const BrowserEntry &b){return a.name<b.name;});
    return out;
}
} // namespace ulg
