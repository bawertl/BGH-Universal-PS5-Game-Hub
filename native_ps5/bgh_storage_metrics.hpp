// SPDX-License-Identifier: MIT
#pragma once
#include <cmath>
#include <cerrno>
#include <initializer_list>
#include <cstdint>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>

namespace ulg::bgh_storage {
struct Volume {
    std::string mount;
    std::uint64_t capacity_bytes=0;
    std::uint64_t used_bytes=0;
    std::uint64_t available_bytes=0;
    bool known=false;
    int error=0;
    double percent() const {
        return known && capacity_bytes ? 100.0*static_cast<double>(used_bytes)/
               static_cast<double>(capacity_bytes) : 0.0;
    }
};
inline Volume probe(const char* mount) {
    Volume m{};
    m.mount=mount;
    struct statvfs fs{};
    if (::statvfs(mount,&fs) != 0) {m.error=errno;return m;}
    const std::uint64_t blocksize=fs.f_frsize ? fs.f_frsize : fs.f_bsize;
    if (!blocksize || !fs.f_blocks) return m;
    m.capacity_bytes = blocksize * static_cast<std::uint64_t>(fs.f_blocks);
    const std::uint64_t free_blocks = fs.f_bfree > fs.f_blocks ? fs.f_blocks : fs.f_bfree;
    m.used_bytes = blocksize * (static_cast<std::uint64_t>(fs.f_blocks)-free_blocks);
    m.available_bytes = blocksize * static_cast<std::uint64_t>(fs.f_bavail);
    m.known=true;
    return m;
}
inline Volume internal() {
    Volume user=probe("/user");
    return user.known ? user : probe("/data");
}
inline Volume external() {
    // A mount directory can exist without media inserted. Reject that case
    // by comparing device IDs with /mnt. If the system uses same-device bind
    // mounts, reporting 'unknown' is safer than showing false USB capacity.
    struct stat parent{};
    if (::stat("/mnt",&parent) != 0) return {};
    for (const char* path : {"/mnt/usb0","/mnt/usb1","/mnt/usb2","/mnt/usb3"}) {
        struct stat st{};
        if (::stat(path,&st) != 0 || !S_ISDIR(st.st_mode) || st.st_dev == parent.st_dev) continue;
        Volume v=probe(path);
        if (v.known) return v;
    }
    return {};
}
inline std::string format_gb(std::uint64_t bytes) {
    char buf[48]{};
    std::snprintf(buf,sizeof(buf),"%.1f GB",static_cast<double>(bytes)/1000000000.0);
    return buf;
}
inline std::string label(const Volume& v) {
    if (!v.known) return "Nicht verfuegbar";
    const auto used=format_gb(v.used_bytes),cap=format_gb(v.capacity_bytes);
    char percent[24]{};
    std::snprintf(percent,sizeof(percent),"%.0f%%",v.percent());
    return used + " / " + cap + " (" + percent + ")";
}
} // namespace ulg::bgh_storage
