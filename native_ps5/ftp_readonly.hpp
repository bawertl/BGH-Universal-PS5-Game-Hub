// PS5 Universal Game Launcher - optional read-only loopback ftpsrv access.
// No STOR/DELE/RMD/MKD, no payload start; only localhost:2121 by default.
// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace ulg {
struct FtpEntry {std::string name;bool folder=false;};
struct FtpListing {
    bool ok=false;
    int reply=0; // FTP response, NOT errno
    std::string operation;
    std::vector<FtpEntry> entries;
};
struct FtpTransferStatus {
    bool ok=false;
    int reply=0;
    std::size_t received=0;
    std::size_t expected=0; // 0 means FTP SIZE unavailable
    std::string message;
};
class FtpReadonly {
public:
    explicit FtpReadonly(int port=2121);
    ~FtpReadonly();
    FtpReadonly(const FtpReadonly&)=delete;
    FtpReadonly& operator=(const FtpReadonly&)=delete;
    bool connect_local();
    FtpListing list(const std::string &directory,std::size_t max_entries=2000);
    bool retrieve(const std::string &path,std::string &bytes,std::size_t max_bytes=262144);
    FtpTransferStatus retrieve_detailed(const std::string &path,std::string &bytes,std::size_t max_bytes=262144);
    // Read exactly len bytes from an absolute file offset using REST/RETR.
    // A new connection should be used per range because RETR is intentionally aborted
    // after the requested bytes; this never sends a mutating FTP command.
    bool retrieve_range(const std::string &path,std::uint64_t offset,std::string &bytes,std::size_t len);
    bool connected() const {return fd_>=0;}
    int last_retr_reply() const {return last_retr_reply_;}
private:
    void disconnect();
    int fd_=-1;
    int port_=2121;
    int last_retr_reply_=0;
    std::string pending_;
    int response(std::string &line);
    int command(const std::string &line,std::string &reply);
    int data_socket(std::string &error);
    bool read_data(int data,std::string &out,std::size_t max_size);
};
} // namespace ulg
