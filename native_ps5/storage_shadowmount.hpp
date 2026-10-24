// BGH V1.1.14: read-only ShadowMountPlus 1.7 HTTP storage telemetry.
// No direct PS5 statvfs, no service configuration or storage mutation.
// This experimental first integration uses the existing, bounded localhost
// socket model; PS5 timing and API availability still require console tests.
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

namespace ulg::shadow_storage {
struct Volume {
    bool valid=false;
    std::uint64_t total=0, used=0, available=0;
};
struct Snapshot {
    Volume internal{}, usb{};
    unsigned usb_count=0;
    bool http_ok=false;
    std::string diagnostic="NO_REQUEST";
};
namespace detail {
inline bool digit(char c){return c>='0' && c<='9';}
inline void space(const std::string &s,std::size_t &p){
    while(p<s.size()&&(s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t'))++p;
}
inline bool quoted(const std::string &s,std::size_t &p,std::string &out){
    space(s,p);if(p>=s.size()||s[p++]!='"')return false;
    out.clear();
    while(p<s.size()){
        char c=s[p++];
        if(c=='"')return true;
        if(static_cast<unsigned char>(c)<32)return false;
        if(c=='\\'){
            if(p>=s.size())return false;
            c=s[p++];
            // Mount points and schema field names are ASCII. Reject escaped
            // unicode rather than silently interpreting a different path.
            if(c!='"'&&c!='\\'&&c!='/')return false;
        }
        if(out.size()>=1024)return false;
        out+=c;
    }
    return false;
}
inline bool integer(const std::string &s,std::size_t &p,std::uint64_t &out){
    space(s,p);if(p>=s.size()||!digit(s[p]))return false;
    std::uint64_t v=0;
    do {
        const unsigned d=static_cast<unsigned>(s[p++]-'0');
        if(v>(std::numeric_limits<std::uint64_t>::max()-d)/10)return false;
        v=v*10+d;
    }while(p<s.size()&&digit(s[p]));
    if(p<s.size()&&(s[p]=='.'||s[p]=='e'||s[p]=='E'))return false;
    out=v;return true;
}
inline bool boolean(const std::string &s,std::size_t &p,bool &v){
    space(s,p);if(s.compare(p,4,"true")==0){v=true;p+=4;return true;}
    if(s.compare(p,5,"false")==0){v=false;p+=5;return true;}
    return false;
}
inline bool skip(const std::string &s,std::size_t &p,unsigned depth=0){
    space(s,p);if(p>=s.size()||depth>12)return false;
    if(s[p]=='"'){std::string value;return quoted(s,p,value);}
    if(s[p]=='{'||s[p]=='['){
        const char start=s[p++],end=start=='{'?'}':']';
        space(s,p);if(p<s.size()&&s[p]==end){++p;return true;}
        while(p<s.size()){
            if(start=='{'){
                std::string key;if(!quoted(s,p,key))return false;
                space(s,p);if(p>=s.size()||s[p++]!=':')return false;
            }
            if(!skip(s,p,depth+1))return false;
            space(s,p);if(p<s.size()&&s[p]==end){++p;return true;}
            if(p>=s.size()||s[p++]!=',')return false;
        }
        return false;
    }
    if(s.compare(p,4,"null")==0){p+=4;return true;}
    bool b=false;if(boolean(s,p,b))return true;
    std::uint64_t n=0;return integer(s,p,n);
}
struct Mount {
    std::string source,mount,path_type;
    std::uint64_t total=0, used=0, available=0;
    unsigned fields=0;
};
inline bool parse_mount(const std::string &s,std::size_t &p,Mount &m){
    space(s,p);if(p>=s.size()||s[p++]!='{')return false;
    bool closed=false;
    while(p<s.size()){
        space(s,p);if(p<s.size()&&s[p]=='}'){++p;closed=true;break;}
        std::string key;if(!quoted(s,p,key))return false;
        space(s,p);if(p>=s.size()||s[p++]!=':')return false;
        if(key=="mount_point"||key=="source"||key=="filesystem"){
            std::string val;if(!quoted(s,p,val))return false;
            if(key=="mount_point"){m.mount=val;m.fields|=1;}
            if(key=="source"){m.source=val;m.fields|=2;}
            if(key=="filesystem")m.path_type=val;
        }else if(key=="total_bytes"||key=="used_bytes"||key=="available_bytes"){
            std::uint64_t val=0;if(!integer(s,p,val))return false;
            if(key=="total_bytes"){m.total=val;m.fields|=4;}
            if(key=="used_bytes"){m.used=val;m.fields|=8;}
            if(key=="available_bytes"){m.available=val;m.fields|=16;}
        }else if(!skip(s,p))return false;
        space(s,p);
        if(p<s.size()&&s[p]=='}'){++p;closed=true;break;}
        if(p>=s.size()||s[p++]!=',')return false;
    }
    return closed && m.fields==31 && !m.source.empty() && m.mount.size()>1 &&
        m.mount[0]=='/' && m.total>0 && m.used<=m.total && m.available<=m.total;
}
inline bool mount_usb(const std::string &s){
    // Documented ShadowMountPlus roots. Do not sum unrelated mounts.
    for(const auto* prefix:{"/mnt/usb","/mnt/ext"}){
        const std::string pre(prefix);
        if(s.compare(0,pre.size(),pre)==0 && s.size()==pre.size()+1 &&
            digit(s.back()))return true;
    }
    return false;
}
inline bool add(Volume &v,const Mount &m){
    if(v.total>std::numeric_limits<std::uint64_t>::max()-m.total ||
       v.used>std::numeric_limits<std::uint64_t>::max()-m.used ||
       v.available>std::numeric_limits<std::uint64_t>::max()-m.available)return false;
    v.total+=m.total;v.used+=m.used;v.available+=m.available;v.valid=true;
    return true;
}
} // namespace detail
inline bool parse_storage_json(const std::string &json,Snapshot &out){
    Snapshot result{};
    if(json.size()>131072)return false;
    std::size_t p=0;detail::space(json,p);if(p>=json.size()||json[p++]!='{')return false;
    bool ok_status=false,got_mounts=false,closed_root=false;
    std::set<std::string> devices;
    while(p<json.size()){
        detail::space(json,p);if(p<json.size()&&json[p]=='}'){++p;closed_root=true;break;}
        std::string key;if(!detail::quoted(json,p,key))return false;
        detail::space(json,p);if(p>=json.size()||json[p++]!=':')return false;
        if(key=="status"){
            std::uint64_t status=1;if(!detail::integer(json,p,status)||status!=0)return false;
            ok_status=true;
        }else if(key=="mounts"){
            detail::space(json,p);if(p>=json.size()||json[p++]!='[')return false;
            got_mounts=true;bool closed_array=false;
            unsigned entries=0;
            while(p<json.size()){
                detail::space(json,p);if(p<json.size()&&json[p]==']'){++p;closed_array=true;break;}
                if(++entries>64)return false;
                detail::Mount m;if(!detail::parse_mount(json,p,m))return false;
                if(m.mount=="/user"){
                    // Internal /user, not the raw drive's advertised capacity.
                    if(!result.internal.valid&&!detail::add(result.internal,m))return false;
                }else if(detail::mount_usb(m.mount)){
                    if(devices.insert(m.source).second){
                        if(!detail::add(result.usb,m))return false;
                        ++result.usb_count;
                    }
                }
                detail::space(json,p);if(p<json.size()&&json[p]==']'){++p;closed_array=true;break;}
                if(p>=json.size()||json[p++]!=',')return false;
            }
            if(!closed_array)return false;
        }else if(!detail::skip(json,p))return false;
        detail::space(json,p);if(p<json.size()&&json[p]=='}'){++p;closed_root=true;break;}
        if(p>=json.size()||json[p++]!=',')return false;
    }
    detail::space(json,p);
    if(!closed_root||!ok_status||!got_mounts||p!=json.size())return false;
    result.http_ok=true;result.diagnostic="OK";out=result;return true;
}
inline bool decode_http(const std::string &response,std::string &body){
    const std::size_t split=response.find("\r\n\r\n");
    if(split==std::string::npos||split>8192)return false;
    const std::size_t line=response.find("\r\n");
    if(line==std::string::npos ||
       (response.compare(0,12,"HTTP/1.1 200")!=0 &&
        response.compare(0,12,"HTTP/1.0 200")!=0))return false;
    const std::string headers=response.substr(0,split);
    if(headers.find("Transfer-Encoding: chunked")!=std::string::npos ||
       headers.find("transfer-encoding: chunked")!=std::string::npos){
        // No speculative decoder on PS5: unsupported transport -> clean fallback.
        return false;
    }
    body=response.substr(split+4);
    if(body.size()>131072)return false;
    const std::size_t pos=headers.find("Content-Length:");
    if(pos!=std::string::npos){
        std::size_t num=pos+15;while(num<headers.size()&&headers[num]==' ')++num;
        if(num==headers.size()||!detail::digit(headers[num]))return false;
        std::uint64_t length=0;
        if(!detail::integer(headers,num,length)||length!=body.size())return false;
    }
    return true;
}
inline Snapshot fetch_storage(){
    Snapshot result{};
    result.diagnostic="CONNECT_ERROR";
    const int fd=::socket(AF_INET,SOCK_STREAM,0);
    if(fd<0){result.diagnostic="SOCKET_ERROR";return result;}
    // Loopback only. An unreachable daemon must not create an unbounded wait.
    timeval timeout{};timeout.tv_sec=0;timeout.tv_usec=200000;
    if(::setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))!=0 ||
       ::setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout))!=0){
        result.diagnostic="TIMEOUT_UNSUPPORTED";::close(fd);return result;
    }
#if !defined(MSG_NOSIGNAL)
# if defined(SO_NOSIGPIPE)
    int nosig=1;
    if(::setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&nosig,sizeof(nosig))!=0){
        result.diagnostic="NOSIGPIPE_UNAVAILABLE";::close(fd);return result;
    }
# else
    result.diagnostic="NOSIGPIPE_UNAVAILABLE";::close(fd);return result;
# endif
#endif
    sockaddr_in address{};address.sin_family=AF_INET;
    address.sin_port=htons(10101);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(::connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0){
        ::close(fd);return result;
    }
    const std::string request="POST /api/v1/storage HTTP/1.0\r\n"
        "Host: 127.0.0.1:10101\r\nContent-Type: application/json\r\n"
        "Content-Length: 2\r\nConnection: close\r\n\r\n{}";
    std::size_t sent=0;
    while(sent<request.size()){
        int flags=0;
#if defined(MSG_NOSIGNAL)
        flags=MSG_NOSIGNAL;
#endif
        const ssize_t n=::send(fd,request.data()+sent,request.size()-sent,flags);
        if(n<0 && errno==EINTR)continue;
        if(n<=0){result.diagnostic="SEND_ERROR";::close(fd);return result;}
        sent+=static_cast<std::size_t>(n);
    }
    std::string response;char data[4096];
    result.diagnostic="RECV_ERROR";
    // A bounded number of reads, each with a bounded socket timeout.
    bool received_eof=false;
    for(unsigned read_count=0;read_count<32;++read_count){
        const ssize_t n=::recv(fd,data,sizeof(data),0);
        if(n<0 && errno==EINTR)continue;
        if(n<0){::close(fd);return result;}
        if(n==0){received_eof=true;break;}
        if(response.size()+static_cast<std::size_t>(n)>131072){
            result.diagnostic="RESPONSE_TOO_LARGE";::close(fd);return result;
        }
        response.append(data,static_cast<std::size_t>(n));
    }
    ::close(fd);
    if(!received_eof){result.diagnostic="HTTP_TRUNCATED";return result;}
    std::string json;
    if(!decode_http(response,json)){result.diagnostic="HTTP_INVALID";return result;}
    if(!parse_storage_json(json,result)){result.diagnostic="JSON_INVALID";return result;}
    return result;
}
} // namespace ulg::shadow_storage
