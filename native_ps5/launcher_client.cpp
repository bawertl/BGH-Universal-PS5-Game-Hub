// SPDX-License-Identifier: MIT
// Minimal protocol client, no sockets opened until the user explicitly requests a launch.
#include "launcher_client.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
namespace ulg {
namespace {
constexpr std::size_t kPacket=0x1020;
constexpr std::uint32_t kMagic=0x4C574650u;
void put_u32(unsigned char *p,std::uint32_t v){for(int i=0;i<4;++i)p[i]=static_cast<unsigned char>((v>>(i*8))&0xFF);}
std::uint32_t get_u32(const unsigned char *p){return std::uint32_t(p[0]) | (std::uint32_t(p[1])<<8) | (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24);}
bool write_full(int fd,const unsigned char *data,std::size_t n){
    while(n){const ssize_t written=send(fd,data,n,0);if(written<0 && errno==EINTR)continue;
        if(written<=0)return false;
        data+=written;n-=static_cast<std::size_t>(written);
    }return true;
}
bool read_full(int fd,unsigned char *data,std::size_t n){
    while(n){const ssize_t got=recv(fd,data,n,0);if(got<0&&errno==EINTR)continue;
        if(got<=0)return false;
        data+=got;n-=static_cast<std::size_t>(got);
    }return true;
}
bool suffix(const std::string &name,const std::string &ext){
    if(name.size()<ext.size())return false;
    for(std::size_t i=0;i<ext.size();++i){char a=name[name.size()-ext.size()+i],b=ext[i];
        if(a>='A'&&a<='Z') a=static_cast<char>(a+'a'-'A');
        if(a!=b)return false;
    }return true;
}
} // namespace
const char *probe_stage_name(PortProbeStage stage){
    switch(stage){
    case PortProbeStage::none: return "OK";
    case PortProbeStage::invalid_arguments: return "PARAM";
    case PortProbeStage::socket_create: return "SOCKET";
    case PortProbeStage::connect_call: return "CONNECT";
    }
    return "UNBEKANNT";
}
const char *probe_errno_name(int error){
    switch(error){
    case 0: return "OK";
    case EACCES: return "EACCES";
    case EPERM: return "EPERM";
    case ECONNREFUSED: return "ECONNREFUSED";
    case ETIMEDOUT: return "ETIMEDOUT";
    case EHOSTUNREACH: return "EHOSTUNREACH";
    case ENETUNREACH: return "ENETUNREACH";
    case EADDRNOTAVAIL: return "EADDRNOTAVAIL";
    case EAFNOSUPPORT: return "EAFNOSUPPORT";
    case EINPROGRESS: return "EINPROGRESS";
    case EALREADY: return "EALREADY";
    case EINTR: return "EINTR";
    case EINVAL: return "EINVAL";
    case EBADF: return "EBADF";
    case EMFILE: return "EMFILE";
    case ENFILE: return "ENFILE";
    case ENOMEM: return "ENOMEM";
    case EIO: return "EIO";
    }
    return "ANDERER_FEHLER";
}
LocalPortProbe probe_loopback_tcp(unsigned short port,int timeout_ms){
    LocalPortProbe result;
    if(port==0 || timeout_ms<1 || timeout_ms>1000){
        result.os_error=EINVAL;result.stage=PortProbeStage::invalid_arguments;
        return result;
    }
    const int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0){result.os_error=errno;result.stage=PortProbeStage::socket_create;return result;}
    // BGH V0.12.13: on the real PS5, fcntl(F_GETFL/F_SETFL) returns EACCES.
    // Diagnose the actual blocking connect() path also used by send_launch().
    // Best-effort socket timeouts: BSD does not universally guarantee that
    // SO_SNDTIMEO caps blocking connect(), so this is deliberately localhost only.
    timeval timeout{};
    timeout.tv_sec=timeout_ms/1000;
    timeout.tv_usec=(timeout_ms%1000)*1000;
    (void)setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    (void)setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(connect(fd,reinterpret_cast<sockaddr *>(&addr),sizeof(addr))==0){
        result.reachable=true;
    }else{
        result.os_error=errno;
        result.stage=PortProbeStage::connect_call;
    }
    // No write/send, no ELF transmission, no service control: connect + close only.
    close(fd);
    return result;
}
LocalLaunchServices probe_local_launch_services(){
    LocalLaunchServices probes;
    probes.launcher=probe_loopback_tcp(10199);
    probes.elfldr=probe_loopback_tcp(9021);
    probes.ftp=probe_loopback_tcp(2121);
    return probes;
}

LaunchPlan make_launch_plan(const std::string &platform,const std::string &game_path,const std::string &emulator_title_id){
    LaunchPlan p;
    if(game_path.empty()||game_path.front()!='/'||game_path.size()>4000||game_path.find("/../")!=std::string::npos||game_path.find('\0')!=std::string::npos){
        p.reason="Ungültiger Spielpfad";return p;
    }
    if(platform=="Switch"){
        if(!suffix(game_path,".nsp")&&!suffix(game_path,".xci")){p.reason="Kein bestätigtes Switch-Format";return p;}
        p.target="PPSA99008";p.argv={"--rom",game_path,"--exit-after-game"};p.supported=true;
    }else if(platform=="Xbox 360"){
        p.target="PPSA50011";p.argv={game_path};p.supported=true;
    }else p.reason="Für diese Plattform keine verifizierte Startschnittstelle";
    if(!emulator_title_id.empty()){
        if(emulator_title_id.size()!=9 || emulator_title_id.compare(0,4,"PPSA")!=0 ||
           emulator_title_id=="PPSA99079" ||
           emulator_title_id.find_first_not_of("0123456789",4)!=std::string::npos){
            p.supported=false;p.reason="Ungueltige Emulator-App-ID";
        }else if(p.supported)p.target=emulator_title_id;
    }
    return p;
}
LaunchPlan make_emulator_menu_launch_plan(const std::string &id){
    LaunchPlan p;
    if(id.size()==9 && id.compare(0,4,"PPSA")==0 && id!="PPSA99079" &&
       id.find_first_not_of("0123456789",4)==std::string::npos){p.target=id;p.supported=true;}
    else p.reason="Ungueltige Emulator-App-ID";
    return p;
}
LaunchPlan make_registered_ps5_launch_plan(const std::string &title_id){
    LaunchPlan p;
    if(title_id.size()!=9 || title_id.compare(0,4,"PPSA")!=0){
        p.reason="Keine gueltige native PPSA-Spielekennung";return p;
    }
    for(std::size_t i=4;i<title_id.size();++i){
        if(title_id[i]<'0'||title_id[i]>'9'){
            p.reason="Keine gueltige native PPSA-Spielekennung";return p;
        }
    }
    // The ordinary library must never launch itself or emulator menu entries
    // through the PS5 game path. The emulator menu remains a separate action.
    if(title_id=="PPSA99079" || title_id=="PPSA99008" ||
       title_id=="PPSA99203" || title_id=="PPSA50011" ||
       title_id=="PPSA99186" || title_id=="PPSA99303"){
        p.reason="Homebrew/Emulator ist kein PS5-Spiel";return p;
    }
    p.target=title_id;
    p.supported=true;
    // No path/ROM arguments: the daemon receives the registered title only.
    return p;
}
LaunchPlan make_registered_ps4_launch_plan(const std::string &title_id){
    LaunchPlan p;
    if(title_id.size()!=9 || title_id.compare(0,4,"CUSA")!=0){
        p.reason="Keine gueltige CUSA-Spielekennung";return p;
    }
    for(std::size_t i=4;i<title_id.size();++i){
        if(title_id[i]<'0'||title_id[i]>'9'){
            p.reason="Keine gueltige CUSA-Spielekennung";return p;
        }
    }
    p.target=title_id;
    p.supported=true;
    // Only the registered CUSA title; no path and no emulator arguments.
    return p;
}
LaunchPlan make_ps5sx2_menu_launch_plan(){
    LaunchPlan p;
    p.target="PPSA99203";
    p.supported=true;
    // The current public PS5SX2 main() has no argc/argv; do not imply
    // that selecting a PS2 game here also starts that disc image.
    // The existing, already running Forwarder on port 10199 does the app switch.
    return p;
}
LaunchPlan make_prosperoeden_menu_launch_plan(){
    LaunchPlan p;
    p.target="PPSA99008";
    p.supported=true;
    // An empty argv asks the existing forwarder to open the app menu only.
    return p;
}
LaunchPlan make_ps5x360_menu_launch_plan(){
    LaunchPlan p;
    p.target="PPSA50011";
    p.supported=true;
    return p;
}
LaunchResponse send_launch(const LaunchPlan &plan,unsigned short port){
    LaunchResponse response;
    if(!plan.supported||plan.target.size()!=9){response.status=LaunchStatus::invalid_target;response.message="Start nicht unterstützt";return response;}
    if(plan.argv.size()>64){response.status=LaunchStatus::invalid_path;response.message="Zu viele Argumente";return response;}
    unsigned char req[kPacket]{};
    put_u32(req,kMagic);put_u32(req+4,1);
    std::memcpy(req+8,plan.target.data(),9);
    std::size_t used=0;
    for(const auto &arg:plan.argv){
        if(arg.find('\0')!=std::string::npos||used+arg.size()+1>4096){response.status=LaunchStatus::invalid_path;response.message="Argument zu lang";return response;}
        std::memcpy(req+0x20+used,arg.data(),arg.size());used+=arg.size()+1;
    }
    put_u32(req+0x18,static_cast<std::uint32_t>(plan.argv.size()));
    put_u32(req+0x1C,static_cast<std::uint32_t>(used));
    const int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0){response.status=LaunchStatus::no_launcher;response.message="Socket nicht verfügbar";return response;}
    timeval timeout{};timeout.tv_sec=1;timeout.tv_usec=0;
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(connect(fd,reinterpret_cast<sockaddr *>(&address),sizeof(address))!=0){
        close(fd);response.status=LaunchStatus::no_launcher;response.message="Kein Startdienst auf 127.0.0.1";return response;
    }
    unsigned char reply[0x18]{};
    const bool ok=write_full(fd,req,sizeof(req))&&read_full(fd,reply,sizeof(reply));
    close(fd);
    if(!ok||get_u32(reply)!=kMagic||get_u32(reply+4)!=1){
        response.status=LaunchStatus::protocol_error;response.message="Ungültige Antwort des Startdienstes";return response;
    }
    response.stage=get_u32(reply+8);response.rc1=get_u32(reply+0x0C);response.rc2=get_u32(reply+0x10);
    if(response.stage==0){response.status=LaunchStatus::started;response.message="Spielstart vom Dienst bestätigt";}
    else if(response.stage==3 && response.rc1==0x80940010u && response.rc2==0x80940010u){
        response.status=LaunchStatus::queued;response.message="Spielstart vom Dienst eingereiht";
    }else{
        response.status=LaunchStatus::refused;
        response.message=response.stage==2?"Kein Benutzer angemeldet":"Startdienst lehnt den Start ab";
    }
    return response;
}
} // namespace ulg
