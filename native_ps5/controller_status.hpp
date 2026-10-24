// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#if defined(BGH_TARGET_PS5)
#include <dlfcn.h>
#endif
namespace ulg {
// UI kit owns and reads controller 1. All auxiliary probes are optional;
// they never close a borrowed UI-kit Pad handle or claim unknown battery data.
class ControllerStatus {
public:
    // -1 unknown, 0 disconnected (verified), 1 connected.
    std::array<int,4> state{{-1,-1,-1,-1}};
    std::array<int,4> battery{{-1,-1,-1,-1}};
    std::array<int,4> charging{{-1,-1,-1,-1}};
    // Diagnostic fields are last return values, not connected/battery evidence.
    // -999 means no call was made. Read-state struct offsets remain unverified
    // for firmware 13.60 and need a physical two-pad test.
    std::array<int,4> last_get{{-999,-999,-999,-999}};
    std::array<int,4> last_open{{-999,-999,-999,-999}};
    std::array<int,4> last_read{{-999,-999,-999,-999}};
    // Passive diagnostics: captured only from calls already made by poll().
    // They are observations, NOT proof of pad ownership or physical ports.
    unsigned poll_count=0;
    bool symbols_ready=false;
    int last_initial_result=-999;
    std::int32_t last_initial_user=-1;
    int last_login_result=-999;
    std::array<std::int32_t,4> last_login_users{{-1,-1,-1,-1}};
    static std::string diagnostic_number(int value){
        return value==-999?std::string("--"):std::to_string(value);
    }
    std::string diagnostic_users() const {
        std::string result="init="+diagnostic_number(last_initial_result)+
            "/"+std::to_string(last_initial_user)+
            " list="+diagnostic_number(last_login_result)+" [";
        for(unsigned i=0;i<4;++i){
            if(i)result+=",";
            result+=std::to_string(last_login_users[i]);
        }
        return result+"]";
    }
    std::string diagnostic_pad(unsigned slot) const {
        if(slot>=4)return "?";
        const char *status=state[slot]==1?"ON":(state[slot]==0?"OFF":"UNKNOWN");
        return "C"+std::to_string(slot+1)+" "+status+
            " G/O/R="+probe_summary(slot);
    }
    std::string probe_summary(unsigned slot) const {
        if(slot>=4)return "?";
        const auto code=[](int value){return value==-999?std::string("--"):std::to_string(value);};
        return code(last_get[slot])+"/"+code(last_open[slot])+"/"+code(last_read[slot]);
    }
    ControllerStatus()=default;
    ControllerStatus(const ControllerStatus&)=delete;
    ControllerStatus& operator=(const ControllerStatus&)=delete;
    void observe_primary(bool connected){
        state[0]=connected?1:0;
        if(!connected){battery[0]=-1;charging[0]=-1;}
    }
    void poll(){
#if defined(BGH_TARGET_PS5)
        ++poll_count;
        if(!resolved_){
            initial_=reinterpret_cast<Initial>(dlsym(RTLD_DEFAULT,"sceUserServiceGetInitialUser"));
            users_=reinterpret_cast<Users>(dlsym(RTLD_DEFAULT,"sceUserServiceGetLoginUserIdList"));
            get_=reinterpret_cast<Get>(dlsym(RTLD_DEFAULT,"scePadGetHandle"));
            open_=reinterpret_cast<Open>(dlsym(RTLD_DEFAULT,"scePadOpen"));
            read_=reinterpret_cast<Read>(dlsym(RTLD_DEFAULT,"scePadReadState"));
            close_=reinterpret_cast<Close>(dlsym(RTLD_DEFAULT,"scePadClose"));
            resolved_=(get_ && read_ && (initial_ || users_));
        }
        symbols_ready=resolved_;
        last_initial_result=-999;
        last_initial_user=-1;
        last_login_result=-999;
        last_login_users.fill(-1);
        // Keep controller 1 set by UI-kit observation; no overwrites of its
        // status or reads from its live input queue.
        for(unsigned slot=1;slot<4;++slot){
            state[slot]=-1;
            last_get[slot]=-999;
            last_open[slot]=-999;
            last_read[slot]=-999;
            battery[slot]=-1;
            charging[slot]=-1;
        }
        if(!resolved_)return;
        std::int32_t initial_user=-1;
        if(initial_){
            last_initial_result=initial_(&initial_user);
            if(last_initial_result!=0)initial_user=-1;
        }
        last_initial_user=initial_user;
        std::array<std::int32_t,4> logged{{-1,-1,-1,-1}};
        if(users_){
            last_login_result=users_(logged.data());
            if(last_login_result!=0)logged.fill(-1);
        }
        last_login_users=logged;
        if(initial_user<0){
            for(const auto user:logged)if(user>=0){initial_user=user;break;}
        }
        if(initial_user<0)return;

        // scePadGetHandle(user, *type*, index): type 0 is a standard pad.
        // Passing slot as type (the proposed port-based snippet) is incorrect.
        // Additional indices may be unsupported on particular PS5 builds;
        // leave those slots unknown rather than fabricate disconnected pads.
        for(int index=1;index<4;++index){
            const int status=read_pad(initial_user,0,index,index);
            if(status>=0)state[static_cast<unsigned>(index)]=status;
        }
        // Other signed-in users can own additional pads. Place confirmed
        // connected pads in remaining empty slots without touching the UI pad.
        for(const auto user:logged){
            if(user<0 || user==initial_user)continue;
            const int status=read_pad(user,0,0,-1);
            if(status!=1)continue;
            for(unsigned slot=1;slot<4;++slot){
                if(state[slot]!=1){state[slot]=1;break;}
            }
        }
#endif
    }
private:
#if defined(BGH_TARGET_PS5)
    using Initial=int(*)(std::int32_t*);
    using Users=int(*)(std::int32_t*);
    using Get=int(*)(std::int32_t,int,int);
    using Open=int(*)(std::int32_t,int,int,const void*);
    using Read=int(*)(int,void*);
    using Close=int(*)(int);
    Initial initial_=nullptr;
    Users users_=nullptr;
    Get get_=nullptr;
    Open open_=nullptr;
    Read read_=nullptr;
    Close close_=nullptr;
    bool resolved_=false;
    int read_pad(std::int32_t user,int type,int index,int diagnostic_slot){
        int handle=get_(user,type,index);
        if(diagnostic_slot>=0)last_get[static_cast<unsigned>(diagnostic_slot)]=handle;
        bool owned=false;
        if(handle<0 && open_ && close_){
            handle=open_(user,type,index,nullptr);
            if(diagnostic_slot>=0)last_open[static_cast<unsigned>(diagnostic_slot)]=handle;
            owned=handle>=0;
        }
        if(handle<0)return -1;
        alignas(8) unsigned char sample[120]{};
        const int result=read_(handle,sample);
        if(diagnostic_slot>=0)last_read[static_cast<unsigned>(diagnostic_slot)]=result;
        if(owned)close_(handle);
        if(result!=0)return -1;
        std::int32_t connected=0;
        std::memcpy(&connected,sample+0x4c,sizeof(connected));
        return connected!=0?1:0;
    }
#endif
};
}
