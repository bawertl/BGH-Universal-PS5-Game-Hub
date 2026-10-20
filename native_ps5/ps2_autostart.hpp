// BGH PS2 one-shot boot request for the existing, official PS5SX2 disc-launch.txt reader.
// No emulator modifications, no payload installs, no game writes.
// SPDX-License-Identifier: MIT
#pragma once
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include "ftp_ps2_writer.hpp"

namespace ulg {
inline constexpr const char* kPs2DiscLaunchPath = "/data/PCSX2/logs/disc-launch.txt";

struct Ps2DiscLaunchTicket {
    bool ready = false;
    int os_error = 0;
    std::string message;
    std::string request_path;
    std::string payload;
    int ftp_reply = 0;
};

inline bool valid_ps5sx2_disc_path(const std::string& path) {
    // PS5SX2 main-boot.cpp reads the first line into char buf[1024].
    if (path.empty() || path.size() > 1022 || path.front() != '/' ||
        path.find_first_of("\r\n") != std::string::npos || path.find('\0') != std::string::npos ||
        path.find("/../") != std::string::npos || path.find("/./") != std::string::npos ||
        path.find("\\") != std::string::npos || path.back() == '/') return false;
    // Only known, local PS2 media roots: never accept arbitrary system files.
    const bool usb = path.size() > 10 && path.compare(0, 8, "/mnt/usb") == 0 &&
        path[8] >= '0' && path[8] <= '7' && path[9] == '/';
    const bool internal = path.compare(0, 18, "/data/PCSX2/games/") == 0;
    if (!usb && !internal) return false;
    auto ends = [&](const char* ext) {
        const std::size_t n = std::strlen(ext);
        if (path.size() <= n) return false;
        for (std::size_t i = 0; i < n; ++i) {
            char c = path[path.size() - n + i];
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
            if (c != ext[i]) return false;
        }
        return true;
    };
    return ends(".iso") || ends(".chd") || ends(".cso") || ends(".zso");
}

// PS2 only: use the same global FTP file view verified by manual WinSCP upload.
// NO POSIX fallback, which may write inside BGH's private PS5 sandbox.
inline Ps2DiscLaunchTicket stage_ps5sx2_disc_launch(const std::string& game_path,
    const std::string& request_path = kPs2DiscLaunchPath, int ftp_port=2121) {
    Ps2DiscLaunchTicket ticket;
    ticket.request_path=request_path;
    if(request_path!=kPs2DiscLaunchPath || !valid_ps5sx2_disc_path(game_path)){
        ticket.message="Ungueltiger PS2-ISO/CHD-Pfad oder Startdatei";
        ticket.os_error=EINVAL;
        return ticket;
    }
    ticket.payload=game_path+"\n";
    const auto uploaded=ftp_stage_ps2_request(ticket.payload,ftp_port);
    ticket.ready=uploaded.ok;
    ticket.ftp_reply=uploaded.ftp_reply;
    ticket.message=uploaded.message;
    // OS errno has no meaning for FTP return codes.
    return ticket;
}

// On failed PFWL launch remove only if the current remote bytes still match
// this launch's payload. A different or already consumed file is never deleted.
inline bool cancel_ps5sx2_disc_launch(const Ps2DiscLaunchTicket& ticket,int ftp_port=2121){
    if(!ticket.ready || ticket.request_path!=kPs2DiscLaunchPath)return false;
    return ftp_cancel_ps2_request_if_identical(ticket.payload,ftp_port);
}
} // namespace ulg
