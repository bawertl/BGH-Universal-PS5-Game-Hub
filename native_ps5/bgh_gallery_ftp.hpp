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
struct DirectImportReport {
    int readable = 0;
    int cached = 0;
    int cache_errors = 0;
    int directory_errors = 0;
    int upload_errno = 0;
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
    FILE* fp=::fopen(path.c_str(),"rb");
    if(!fp)return false;
    unsigned char signature[8]{};
    const bool ok=::fread(signature,1,sizeof(signature),fp)==sizeof(signature);
    ::fclose(fp);
    const auto point=name.find_last_of('.');
    std::string ext=point==std::string::npos?"":name.substr(point);
    for(char& c:ext)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    const unsigned char png[]={137,80,78,71,13,10,26,10};
    return ok && (ext==".png" ? std::memcmp(signature,png,8)==0 :
            signature[0]==255&&signature[1]==216&&signature[2]==255);
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
// Primary import path: read directly via fopen instead of stat/lstat.
// The fallback FTP code is only needed if this installation path is sandboxed.
inline DirectImportReport import_from_local_filesystem(Kind kind,std::size_t max_new=16){
    DirectImportReport report;
    const char* roots[]={upload_folder(kind),app0_upload_folder(kind),legacy_upload_folder(kind)};
    for(const char* root:roots){
        errno=0;
        DIR* dp=::opendir(root);
        if(!dp){
            ++report.directory_errors;
            if(root==roots[0])report.upload_errno=errno;
            continue;
        }
        while(dirent* entry=::readdir(dp)){
            const std::string name(entry->d_name);
            if(!valid_filename(name))continue;
            const std::string file=std::string(root)+"/"+name;
            FILE* fp=::fopen(file.c_str(),"rb");
            if(!fp)continue;
            unsigned char header[8]{};
            const bool readable=::fread(header,1,sizeof(header),fp)==sizeof(header);
            ::fclose(fp);
            const unsigned char png[]={137,80,78,71,13,10,26,10};
            const auto dot=name.find_last_of('.');
            std::string ext=name.substr(dot);
            for(char& c:ext)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
            if(!readable || !(ext==".png"? std::memcmp(header,png,8)==0 :
                header[0]==255&&header[1]==216&&header[2]==255))continue;
            ++report.readable;
            if(imported_file_exists(kind,name))continue;
            if(report.cached>=static_cast<int>(max_new))continue;
            fp=::fopen(file.c_str(),"rb");
            if(!fp){++report.cache_errors;continue;}
            std::string bytes;
            char buffer[8192];
            bool complete=false;
            while(bytes.size()<=8u*1024u*1024u){
                const std::size_t space=8u*1024u*1024u-bytes.size();
                const std::size_t got=::fread(buffer,1,std::min(sizeof(buffer),space+1),fp);
                if(got>space)break;
                bytes.append(buffer,got);
                if(got<sizeof(buffer)){
                    if(::feof(fp))complete=true;
                    break;
                }
            }
            const bool closed=::fclose(fp)==0;
            if(!complete || !closed || !store_private_image(kind,name,bytes))
                ++report.cache_errors;
            else ++report.cached;
        }
        ::closedir(dp);
    }
    report.detail="Direkt: "+std::to_string(report.readable)+" Bilder, "+
        std::to_string(report.cached)+" kopiert";
    if(report.cache_errors)report.detail+=", Cache-Fehler "+std::to_string(report.cache_errors);
    if(!report.readable)report.detail+=", Upload errno "+std::to_string(report.upload_errno);
    return report;
}
inline ImportReport import_from_local_ftp(Kind kind,std::size_t max_new=8,int port=2121) {
    ImportReport result;
    // The user opened the gallery explicitly. The UI does not call this
    // from rendering or boot, and the existing FTP client enforces timeouts.
    FtpReadonly ftp(port);
    auto listing=ftp.list(upload_folder(kind),72);
    // Some local PS5 FTP servers support NLST but not MLSD. Also handle
    // successfully completed but unparseable MLSD responses (zero entries).
    if(!listing.ok || listing.entries.empty()) {
        auto names=ftp.list_names(upload_folder(kind),72);
        if(names.ok)listing=std::move(names);
        else if(!listing.ok)listing=std::move(names);
    }
    if(!listing.ok){
        result.detail="FTP: "+listing.operation+" Antwort "+std::to_string(listing.reply);
        return result;
    }
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
    result.detail="FTP: "+std::to_string(result.added)+" neu, "+
                  std::to_string(result.already)+" vorhanden";
    if(result.failed)result.detail+=", "+std::to_string(result.failed)+" nicht importiert (max. 8 MB/Bild)";
    return result;
}
}} // namespace ulg::bgh_gallery
