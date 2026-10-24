// PS5 Universal Game Launcher: isolated FTP writer for the official PS5SX2 request.
// Only connects to 127.0.0.1; never changes FTP scanning or game files.
// SPDX-License-Identifier: MIT
#include "ftp_ps2_writer.hpp"
#include "ftp_readonly.hpp"
#include <arpa/inet.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace ulg {
namespace {
constexpr const char* kDirectory="/data/PCSX2/logs";
constexpr const char* kFile="disc-launch.txt";
constexpr const char* kAbsolute="/data/PCSX2/logs/disc-launch.txt";

int open_local(int port) {
    if (port<1 || port>65535) return -1;
    int fd=::socket(AF_INET, SOCK_STREAM, 0);
    if (fd<0) return -1;
    timeval limit{2,0};
    (void)::setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&limit,sizeof(limit));
    (void)::setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&limit,sizeof(limit));
    sockaddr_in addr{};addr.sin_family=AF_INET;
    addr.sin_port=htons(static_cast<std::uint16_t>(port));
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if (::connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0) {
        ::close(fd); return -1;
    }
    return fd;
}
bool send_all(int fd,const std::string& s) {
    std::size_t sent=0;
    while (sent<s.size()) {
        ssize_t count=::send(fd,s.data()+sent,s.size()-sent,0);
        if(count<0 && errno==EINTR) continue;
        if(count<=0) return false;
        sent+=static_cast<std::size_t>(count);
    }
    return true;
}
class FtpSession {
public:
    explicit FtpSession(int port):port_(port){}
    ~FtpSession(){if(fd_>=0)::close(fd_);}
    FtpSession(const FtpSession&)=delete;
    FtpSession& operator=(const FtpSession&)=delete;
    bool login(){
        fd_=open_local(port_);
        if(fd_<0 || response()!=220) return false;
        int r=command("USER anonymous");
        if(r!=230 && (r!=331 || command("PASS ")!=230))return false;
        return command("TYPE I")==200 && command(std::string("CWD ")+kDirectory)==250;
    }
    int command(const std::string& value) {
        if(fd_<0 || !send_all(fd_,value+"\r\n"))return 0;
        return response();
    }
    int response() {
        if(fd_<0)return 0;
        for(int lines=0;lines<24;++lines) {
            std::string line;
            for(;;) {
                auto idx=pending_.find('\n');
                if(idx!=std::string::npos){
                    line=pending_.substr(0,idx);
                    pending_.erase(0,idx+1);
                    break;
                }
                if(pending_.size()>8192)return 0;
                char b[512];ssize_t n=::recv(fd_,b,sizeof(b),0);
                if(n<0 && errno==EINTR)continue;
                if(n<=0)return 0;
                pending_.append(b,static_cast<std::size_t>(n));
            }
            if(line.size()>=4 && line[0]>='1' && line[0]<='5' &&
               line[1]>='0' && line[1]<='9' && line[2]>='0' && line[2]<='9' &&
               line[3]==' ')
                return (line[0]-'0')*100+(line[1]-'0')*10+(line[2]-'0');
            // Multiline FTP responses are consumed until the final 'NNN ' line.
        }
        return 0;
    }
    int passive_socket() {
        // Read the actual PASV reply and ignore its untrusted host field.
        // Both control and data sockets MUST connect to 127.0.0.1.
        if(fd_<0 || !send_all(fd_,"PASV\r\n"))return -1;
        std::string reply;
        for(int lines=0;lines<24;++lines){
            for(;;) {
                const auto idx=pending_.find('\n');
                if(idx!=std::string::npos){
                    reply=pending_.substr(0,idx);
                    pending_.erase(0,idx+1);
                    break;
                }
                if(pending_.size()>8192)return -1;
                char b[512];ssize_t n=::recv(fd_,b,sizeof(b),0);
                if(n<0 && errno==EINTR)continue;
                if(n<=0)return -1;
                pending_.append(b,static_cast<std::size_t>(n));
            }
            if(reply.size()>=4 && reply.compare(0,4,"227 ")==0)break;
            if(reply.size()>=4 && reply[3]==' ' && reply.compare(0,3,"227")!=0)return -1;
        }
        if(reply.compare(0,4,"227 ")!=0)return -1;
        auto a=reply.find('(');auto b=reply.find(')',a);
        if(a==std::string::npos || b==std::string::npos)return -1;
        int v[6]={};
        if(std::sscanf(reply.substr(a+1,b-a-1).c_str(),"%d,%d,%d,%d,%d,%d",
                       &v[0],&v[1],&v[2],&v[3],&v[4],&v[5])!=6)return -1;
        for(int num:v)if(num<0 || num>255)return -1;
        return open_local(v[4]*256+v[5]);
    }
private:
    int fd_=-1,port_=2121;
    std::string pending_;
};

bool valid_payload(const std::string& payload){
    return payload.size()>2 && payload.size()<1024 && payload.back()=='\n' &&
           payload.find('\n')==payload.size()-1 && payload.find('\r')==std::string::npos &&
           payload.find('\0')==std::string::npos;
}

bool shared_request_equals(const std::string& expected,int port){
    FtpReadonly reader(port);
    std::string result;
    return reader.retrieve(kAbsolute,result,1024) && result==expected;
}
} // namespace

Ps2FtpWriteStatus ftp_stage_ps2_request(const std::string& payload,int port) {
    Ps2FtpWriteStatus result;
    if(!valid_payload(payload)){
        result.message="PS2-Startauftrag ungueltig";return result;
    }
    // Authoritative FTP directory listing. Fail closed if it cannot be read.
    // Unlike SIZE alone, this distinguishes 'missing' from unsupported SIZE.
    FtpReadonly scanner(port);
    auto listing=scanner.list(kDirectory,2000);
    if(!listing.ok){
        result.ftp_reply=listing.reply;
        result.message="FTP PS5SX2-Ordner nicht lesbar (MLSD/CWD)";
        return result;
    }
    // The launcher owns only this one-shot PS5SX2 request filename. An old
    // request from a previous session must not prevent the next game launch.
    // Keep the authoritative listing guard, but do not treat an existing
    // regular request file as a permanent error.
    const bool existing=std::any_of(listing.entries.begin(),listing.entries.end(),
        [](const FtpEntry &entry){return entry.name==kFile && !entry.folder;});
    for(const auto &entry:listing.entries){
        if(entry.name==kFile && entry.folder){
            result.ftp_reply=550;
            result.message="PS2-Startdatei ist ein Verzeichnis; nicht veraendert";
            return result;
        }
    }
    FtpSession ftp(port);
    if(!ftp.login()){
        result.message="FTP PS2-Schreibverbindung fehlgeschlagen";return result;
    }
    int data=ftp.passive_socket();
    if(data<0){result.message="FTP PS2-PASV fehlgeschlagen";return result;}
    int start=ftp.command(std::string("STOR ")+kFile);
    result.ftp_reply=start;
    if(start!=125 && start!=150){
        ::close(data);
        // Some FTP servers forbid overwrite but allow deletion of the single
        // fixed request file. Only retry for a known existing request, with an
        // explicit 550 refusal. Never delete any other name or game file.
        if(!existing || start!=550 || ftp.command(std::string("DELE ")+kFile)!=250){
            result.message="FTP STOR abgelehnt; bestehende Datei nicht geaendert";
            return result;
        }
        data=ftp.passive_socket();
        if(data<0){result.message="FTP PS2-PASV nach DELE fehlgeschlagen";return result;}
        start=ftp.command(std::string("STOR ")+kFile);
        result.ftp_reply=start;
        if(start!=125 && start!=150){
            ::close(data);
            result.message="FTP PS2-STOR nach DELE abgelehnt";return result;
        }
    }
    const bool sent=send_all(data,payload);
    // EOF lets the FTP daemon commit the uploaded text file.
    (void)::shutdown(data,SHUT_WR);
    ::close(data);
    if(!sent){result.message="FTP PS2-Datenuebertragung unvollstaendig";return result;}
    const int end=ftp.response();
    result.ftp_reply=end;
    if(end!=226 && end!=250){
        result.message="FTP PS2-STOR nicht bestaetigt";return result;
    }
    // Do not launch the emulator until the SAME FTP daemon can read the exact bytes.
    if(!shared_request_equals(payload,port)){
        result.message="FTP PS2-Datei nicht unveraendert ruecklesbar";return result;
    }
    result.ok=true;
    result.message="PS2-Startauftrag ueber FTP geschrieben und verifiziert";
    return result;
}

bool ftp_cancel_ps2_request_if_identical(const std::string& payload,int port){
    if(!valid_payload(payload) || !shared_request_equals(payload,port))return false;
    FtpSession ftp(port);
    if(!ftp.login())return false;
    return ftp.command(std::string("DELE ")+kFile)==250;
}
} // namespace ulg
