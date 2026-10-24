// SPDX-License-Identifier: MIT
#include "ftp_readonly.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace ulg {
namespace {
int localhost_socket(int port) {
    if(port<1 || port>65535)return -1;
    const int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0)return -1;
    timeval timeout{1,0};
    (void)setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    (void)setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(::connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0){::close(fd);return -1;}
    return fd;
}
bool send_complete(int fd,const std::string &data){
    std::size_t sent=0;
    while(sent<data.size()){
        const ssize_t n=send(fd,data.data()+sent,data.size()-sent,0);
        if(n<=0)return false;
        sent+=static_cast<std::size_t>(n);
    }
    return true;
}
bool safe_ftp_path(const std::string &path){
    return !path.empty() && path[0]=='/' && path.size()<4000 && path.find("\r")==std::string::npos &&
           path.find("\n")==std::string::npos && path.find("/../")==std::string::npos &&
           (path.size()<3 || path.compare(path.size()-3,3,"/..")!=0);
}
}
FtpReadonly::FtpReadonly(int port):port_(port){}
FtpReadonly::~FtpReadonly(){disconnect();}
void FtpReadonly::disconnect(){
    if(fd_>=0)::close(fd_);
    fd_=-1;
    pending_.clear();
}
int FtpReadonly::response(std::string &line){
    if(fd_<0)return 0;
    for(int lines=0;lines<70;++lines){
        while(pending_.find("\n")==std::string::npos && pending_.size()<16384){
            char buf[1024];const ssize_t n=recv(fd_,buf,sizeof(buf),0);
            if(n<=0){disconnect();return 0;}
            pending_.append(buf,static_cast<std::size_t>(n));
        }
        const auto eol=pending_.find('\n');
        if(eol==std::string::npos){disconnect();return 0;}
        line=pending_.substr(0,eol);
        pending_.erase(0,eol+1);
        if(!line.empty() && line.back()=='\r')line.pop_back();
        if(line.size()>=4 && line[0]>='1'&&line[0]<='5' &&
           line[1]>='0'&&line[1]<='9' && line[2]>='0'&&line[2]<='9' && line[3]==' ')
            return (line[0]-'0')*100+(line[1]-'0')*10+(line[2]-'0');
    }
    return 0;
}
int FtpReadonly::command(const std::string &line,std::string &reply){
    if(fd_<0)return 0;
    if(!send_complete(fd_,line+"\r\n")){disconnect();return 0;}
    return response(reply);
}
bool FtpReadonly::connect_local(){
    if(fd_>=0)return true;
    fd_=localhost_socket(port_);if(fd_<0)return false;
    std::string reply;
    if(response(reply)!=220){disconnect();return false;}
    const int login=command("USER anonymous",reply);
    if(login!=230 && (login!=331 || command("PASS ",reply)!=230)){
        disconnect();return false;
    }
    if(command("TYPE I",reply)!=200){disconnect();return false;}
    return true;
}
int FtpReadonly::data_socket(std::string &error){
    std::string reply;
    if(command("PASV",reply)!=227){error="PASV";return -1;}
    const auto open=reply.find('(');const auto close=reply.find(')',open);
    if(open==std::string::npos || close==std::string::npos){error="PASV response";return -1;}
    int values[6]={};
    if(std::sscanf(reply.substr(open+1,close-open-1).c_str(),"%d,%d,%d,%d,%d,%d",
                   &values[0],&values[1],&values[2],&values[3],&values[4],&values[5])!=6)
        return -1;
    for(int v:values)if(v<0||v>255)return -1;
    const int data_port=values[4]*256+values[5];
    // Ignore untrusted PASV host; network access remains STRICTLY loopback.
    return localhost_socket(data_port);
}
bool FtpReadonly::read_data(int fd,std::string &out,std::size_t max_size){
    char buf[8192];
    while(out.size()<max_size){
        const ssize_t n=recv(fd,buf,std::min(sizeof(buf),max_size-out.size()),0);
        if(n==0)return true;
        if(n<0)return false;
        out.append(buf,static_cast<std::size_t>(n));
    }
    char extra=0;
    return recv(fd,&extra,1,0)==0; // reject truncated streams (including huge directories)
}
FtpListing FtpReadonly::list(const std::string &directory,std::size_t max_entries){
    FtpListing out;out.operation="ftp_local";
    if(!safe_ftp_path(directory) || !connect_local())return out;
    std::string reply;
    out.reply=command("CWD "+directory,reply);out.operation="FTP CWD";
    if(out.reply!=250)return out;
    std::string error;
    const int data=data_socket(error);out.operation="FTP PASV";
    if(data<0)return out;
    out.reply=command("MLSD",reply);out.operation="FTP MLSD";
    if(out.reply!=150 && out.reply!=125){::close(data);return out;}
    std::string body;
    const bool data_ok=read_data(data,body,2*1024*1024);
    ::close(data);
    out.reply=response(reply);
    if(!data_ok || out.reply!=226){disconnect();return out;}
    max_entries=std::min(max_entries,std::size_t(5000));
    std::size_t pos=0;
    while(pos<body.size() && out.entries.size()<max_entries){
        const auto end=body.find('\n',pos);
        const std::string line=body.substr(pos,end==std::string::npos?std::string::npos:end-pos);
        pos=end==std::string::npos?body.size():end+1;
        const auto spacer=line.find("; ");
        if(spacer==std::string::npos)continue;
        std::string name=line.substr(spacer+2);
        if(!name.empty() && name.back()=='\r')name.pop_back();
        if(name.empty()||name=="."||name==".."||name.find('/')!=std::string::npos ||
           name.find('\r')!=std::string::npos || name.find('\n')!=std::string::npos)continue;
        const auto facts=line.substr(0,spacer+1);
        if(facts.find("type=dir;")!=std::string::npos)out.entries.push_back({name,true});
        else if(facts.find("type=file;")!=std::string::npos)out.entries.push_back({name,false});
    }
    out.ok=true;out.operation="";out.reply=226;
    return out;
}
// NLST fallback is deliberately separate from the existing MLSD game scanner.
// It does not mutate FTP state outside this object and never sends STOR/DELE.
FtpListing FtpReadonly::list_names(const std::string &directory,std::size_t max_entries){
    FtpListing out;out.operation="FTP CONNECT/LOGIN";
    if(!safe_ftp_path(directory) || !connect_local())return out;
    std::string reply;
    out.reply=command("CWD "+directory,reply);out.operation="FTP CWD";
    if(out.reply!=250)return out;
    std::string error;
    const int data=data_socket(error);out.operation="FTP PASV";
    if(data<0)return out;
    out.reply=command("NLST",reply);out.operation="FTP NLST";
    if(out.reply!=150 && out.reply!=125){::close(data);return out;}
    std::string body;
    const bool received=read_data(data,body,2*1024*1024);
    ::close(data);
    out.reply=response(reply);
    if(!received || out.reply!=226){disconnect();return out;}
    max_entries=std::min(max_entries,std::size_t(5000));
    std::size_t pos=0;
    while(pos<body.size() && out.entries.size()<max_entries){
        const auto end=body.find('\n',pos);
        std::string name=body.substr(pos,end==std::string::npos?std::string::npos:end-pos);
        pos=end==std::string::npos?body.size():end+1;
        if(!name.empty()&&name.back()=='\r')name.pop_back();
        // NLST can return either names or absolute/relative full paths.
        const auto slash=name.find_last_of('/');
        if(slash!=std::string::npos)name=name.substr(slash+1);
        if(name.empty()||name=="."||name==".."||
           name.find('\r')!=std::string::npos||name.find('\n')!=std::string::npos)continue;
        out.entries.push_back({name,false});
    }
    out.ok=true;out.reply=226;out.operation.clear();
    return out;
}
// PS5 metadata/PNG art: retain V0.12's proven RETR-only transaction path.
// SIZE is deliberately NOT used on the shared scan connection.
bool FtpReadonly::retrieve(const std::string &path,std::string &bytes,std::size_t max_bytes){
    bytes.clear();last_retr_reply_=0;
    if(!safe_ftp_path(path) || max_bytes>8u*1024u*1024u || !connect_local())return false;
    std::string error,reply;
    const int data=data_socket(error);
    if(data<0){disconnect();return false;}
    const int code=command("RETR "+path,reply);
    last_retr_reply_=code;
    if(code!=150 && code!=125){
        ::close(data);
        // An explicit 550/5xx reply is a COMPLETE command result, not a
        // corrupted control session. The scanner must try the next cover.
        if(code==0 || code==421)disconnect();
        return false;
    }
    std::string body;
    const bool data_ok=read_data(data,body,max_bytes);
    ::close(data);
    if(!data_ok){disconnect();return false;}
    const int completion=response(reply);
    last_retr_reply_=completion;
    if(completion!=226){disconnect();return false;}
    bytes=std::move(body);
    return true;
}
FtpTransferStatus FtpReadonly::retrieve_detailed(const std::string &path,std::string &bytes,std::size_t max_bytes){
    bytes.clear();
    FtpTransferStatus status;
    if(!safe_ftp_path(path) || max_bytes>8u*1024u*1024u){status.message="UNGUELTIGER FTP-PFAD ODER DATEIGROESSENLIMIT";return status;}
    if(!connect_local()){status.message="FTP-VERBINDUNG/LOGIN/TYPE I FEHLGESCHLAGEN";return status;}
    std::string error,reply;
    // Not all PS5 FTP servers support SIZE. Only validate it if reply is 213.
    const int size_reply=command("SIZE "+path,reply);
    if(size_reply==0){
        status.message="FTP SIZE TIMEOUT/VERBINDUNG VERLOREN";
        disconnect();return status;
    }
    // Only explicit, complete FTP responses (e.g. 500/502/550) may be ignored.
    if(size_reply==213){
        const std::string number=reply.size()>4?reply.substr(4):std::string{};
        if(number.empty() || number.find_first_not_of("0123456789")!=std::string::npos){
            status.reply=size_reply;status.message="FTP SIZE ANTWORT UNGUELTIG";disconnect();return status;
        }
        errno=0;
        char *parsed=nullptr;
        const unsigned long long size=std::strtoull(number.c_str(),&parsed,10);
        if(errno==ERANGE || parsed==nullptr || *parsed!='\0'){
            status.reply=size_reply;status.message="FTP SIZE NICHT LESBAR";disconnect();return status;
        }
        if(size>max_bytes){status.reply=size_reply;status.message="FTP DATEI GROESSER ALS LIMIT";disconnect();return status;}
        status.expected=static_cast<std::size_t>(size);
    }
    const int data=data_socket(error);
    if(data<0){status.message="FTP PASV/DATENVERBINDUNG: "+error;disconnect();return status;}
    const int code=command("RETR "+path,reply);
    status.reply=code;
    if(code!=150 && code!=125){::close(data);status.message="FTP RETR ABGELEHNT";disconnect();return status;}
    char block[8192];
    bool complete=false;
    while(bytes.size()<=max_bytes){
        const std::size_t space=max_bytes-bytes.size();
        const ssize_t n=recv(data,block,std::min(sizeof(block),space+1),0);
        if(n==0){complete=true;break;}
        if(n<0){
            const int code=errno;
            status.message=(code==EAGAIN || code==EWOULDBLOCK)?"FTP DATA TIMEOUT":"FTP DATA SOCKET FEHLER";
            break;
        }
        if(static_cast<std::size_t>(n)>space){status.message="FTP BILD GROESSER ALS LIMIT";break;}
        bytes.append(block,static_cast<std::size_t>(n));
    }
    ::close(data);
    status.received=bytes.size();
    if(!complete){
        if(status.message.empty())status.message="FTP DATENSTROM UNVOLLSTAENDIG";
        bytes.clear();disconnect();return status;
    }
    status.reply=response(reply);
    if(status.reply!=226){status.message="FTP TRANSFER-ABSCHLUSS NICHT 226";bytes.clear();disconnect();return status;}
    if(status.expected && status.expected!=status.received){status.message="FTP SIZE/STROM-LAENGE UNGLEICH";bytes.clear();disconnect();return status;}
    status.ok=true;status.message="OK";
    return status;
}

} // namespace ulg

namespace ulg {
bool FtpReadonly::retrieve_range(const std::string &path,std::uint64_t offset,std::string &bytes,std::size_t len){
    bytes.clear();
    if(len==0 || len>1024*1024 || offset>(std::uint64_t(1)<<55) ||
       !safe_ftp_path(path) || !connect_local())return false;
    std::string error,reply;
    const int data=data_socket(error);
    if(data<0){disconnect();return false;}
    if(offset>0 && command("REST "+std::to_string(offset),reply)!=350){
        ::close(data);disconnect();return false;
    }
    const int code=command("RETR "+path,reply);
    if(code!=150 && code!=125){::close(data);disconnect();return false;}
    bytes.resize(len);
    std::size_t got=0;
    while(got<len){
        const ssize_t n=recv(data,bytes.data()+got,len-got,0);
        if(n<=0)break;
        got+=static_cast<std::size_t>(n);
    }
    ::close(data);
    // The requested window was fully received. Closing the data channel early
    // is expected to terminate this transfer; the control session is discarded.
    disconnect();
    if(got!=len){bytes.clear();return false;}
    return true;
}
} // namespace ulg
