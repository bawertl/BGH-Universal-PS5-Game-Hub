// BGH NextGen UI: read-only storage telemetry, no fabricated capacities.
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#if defined(__has_include)
// The stock PS5 title libc may lack or fault statvfs. Keep it OFF on
// PS5 until an explicit, hardware-validated capability is provided.
# if (defined(__linux__) || defined(__APPLE__) || defined(BGH_VERIFIED_PS5_STATVFS)) && __has_include(<sys/statvfs.h>)
#  include <sys/statvfs.h>
#  define BGH_HAS_STATVFS 1
# endif
#endif
#include <sys/stat.h>

namespace ulg::storage_widgets {
struct Device {
    bool valid=false;
    std::uint64_t total_bytes=0,used_bytes=0;
};
struct Snapshot {
    Device internal{},external{};
    unsigned usb_count=0;
    bool usb_connected=false;
};
inline Device query(const char* path){
    Device out{};
#if defined(BGH_HAS_STATVFS)
    struct stat st{};
    if(::stat(path,&st)!=0 || !S_ISDIR(st.st_mode))return out;
    struct statvfs v{};
    if(::statvfs(path,&v)!=0)return out;
    const std::uint64_t block_size=v.f_frsize?v.f_frsize:v.f_bsize;
    if(!block_size || !v.f_blocks || v.f_bfree>v.f_blocks)return out;
    out.total_bytes=static_cast<std::uint64_t>(v.f_blocks)*block_size;
    out.used_bytes=static_cast<std::uint64_t>(v.f_blocks-v.f_bfree)*block_size;
    out.valid=out.total_bytes!=0;
#else
    (void)path;
#endif
    return out;
}
inline Snapshot detect(){
    Snapshot out{};
    out.internal=query("/user");
    if(!out.internal.valid)out.internal=query("/data");
    // Only a genuinely mounted USB filesystem, not an empty /mnt/usbN folder.
    struct stat base{};
    const bool have_base=::stat("/mnt",&base)==0;
    for(const auto *path:{"/mnt/usb0","/mnt/usb1","/mnt/usb2"}){
        struct stat actual{};
        if(::stat(path,&actual)!=0 || !S_ISDIR(actual.st_mode))continue;
        if(have_base && actual.st_dev==base.st_dev)continue;
        // A distinct mounted directory was observed. No capacity guessed.
        out.usb_connected=true;
        const Device d=query(path);
        if(!d.valid)continue;
        // Multiple mount aliases of same external filesystem must not be summed.
        // BGH has only three ordinary mount candidates; deduplicate st_dev.
        bool duplicate=false;
        for(const auto *previous:{"/mnt/usb0","/mnt/usb1","/mnt/usb2"}){
            if(std::string(previous)==path)break;
            struct stat old{};
            if(::stat(previous,&old)==0 && old.st_dev==actual.st_dev)duplicate=true;
        }
        if(duplicate)continue;
        out.external.valid=true;
        out.external.total_bytes+=d.total_bytes;
        out.external.used_bytes+=d.used_bytes;
        ++out.usb_count;
    }
    return out;
}
inline std::string format_gib(std::uint64_t bytes){
    const double gib=static_cast<double>(bytes)/1000000000.0;
    char result[32]{};
    if(gib>=1000.0)std::snprintf(result,sizeof(result),"%.1f TB",gib/1000.0);
    else std::snprintf(result,sizeof(result),"%.0f GB",gib);
    return result;
}
} // namespace ulg::storage_widgets
