// SPDX-License-Identifier: MIT
#pragma once
// Optional gallery importer via the launcher environment's already-running
// READ-ONLY loopback FTP service. No new payloads, STOR, DELE or PS5 syscalls.
#include "bgh_theme_gallery.hpp"
#include "concepts/ftp_readonly.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace ulg { namespace bgh_gallery {
struct ImportReport {
    bool ftp_ok = false;
    int added = 0;
    int already = 0;
    int failed = 0;
    std::string detail;
};
inline bool image_signature(const std::string& name, const std::string& bytes) {
    if(bytes.size() < 8 || !valid_filename(name)) return false;
    const auto dot = name.find_last_of('.');
    std::string ext = name.substr(dot);
    for(char& c:ext) if(c >= 'A' && c <= 'Z') c=static_cast<char>(c-'A'+'a');
    const unsigned char* data=reinterpret_cast<const unsigned char*>(bytes.data());
    const unsigned char png[] = {137,80,78,71,13,10,26,10};
    if(ext==".png") return std::memcmp(data,png,8)==0;
    return data[0]==0xff && data[1]==0xd8 && data[2]==0xff;
}
inline bool prepare_private_import_folder(Kind kind) {
    const char* root=BGH_GALLERY_CACHE_ROOT;
    if(::mkdir(root,0700)!=0 && errno!=EEXIST) return false;
    const char* folder=imported_folder(kind);
    if(::mkdir(folder,0700)!=0 && errno!=EEXIST) return false;
    return true;
}
inline bool imported_file_exists(Kind kind,const std::string& name) {
    const std::string path=std::string(imported_folder(kind))+"/"+name;
    struct stat st{};
    return ::lstat(path.c_str(),&st)==0 && S_ISREG(st.st_mode) && st.st_size>=8;
}
inline bool store_private_image(Kind kind,const std::string& name,const std::string& bytes) {
    if(!image_signature(name,bytes) || !prepare_private_import_folder(kind))return false;
    const std::string path=std::string(imported_folder(kind))+"/"+name;
    const std::string temp=path+".tmp";
    FILE* fp=::fopen(temp.c_str(),"wb");
    if(!fp)return false;
    const bool written=::fwrite(bytes.data(),1,bytes.size(),fp)==bytes.size() &&
        ::fflush(fp)==0;
    const bool closed=::fclose(fp)==0;
    if(!written || !closed || ::rename(temp.c_str(),path.c_str())!=0) {
        ::remove(temp.c_str());return false;
    }
    return true;
}
inline ImportReport import_from_local_ftp(Kind kind,std::size_t max_new=8,int port=2121) {
    ImportReport result;
    // The user opened the gallery explicitly. The UI does not call this
    // from rendering or boot, and the existing FTP client enforces timeouts.
    FtpReadonly ftp(port);
    const auto listing=ftp.list(upload_folder(kind),72);
    if(!listing.ok){result.detail="FTP-Bilderordner nicht lesbar (Port 2121)";return result;}
    result.ftp_ok=true;
    for(const auto& entry:listing.entries){
        if(entry.folder || !valid_filename(entry.name))continue;
        if(imported_file_exists(kind,entry.name)){++result.already;continue;}
        if(result.added>=static_cast<int>(max_new))break;
        std::string bytes;
        if(!ftp.retrieve(std::string(upload_folder(kind))+"/"+entry.name,bytes,8u*1024u*1024u)
           || !store_private_image(kind,entry.name,bytes)){
            ++result.failed;continue;
        }
        ++result.added;
    }
    result.detail="FTP-Galerie: "+std::to_string(result.added)+" neu, "+
                  std::to_string(result.already)+" vorhanden";
    if(result.failed)result.detail+=", "+std::to_string(result.failed)+" nicht importiert (max. 8 MB/Bild)";
    return result;
}
}} // namespace ulg::bgh_gallery
