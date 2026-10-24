// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
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
    ControllerStatus()=default;
    ControllerStatus(const ControllerStatus&)=delete;
    ControllerStatus& operator=(const ControllerStatus&)=delete;
    void observe_primary(bool connected){
        state[0]=connected?1:0;
        if(!connected){battery[0]=-1;charging[0]=-1;}
    }
    void poll(){
#if defined(BGH_TARGET_PS5)
        if(!resolved_){
            initial_=reinterpret_cast<Initial>(dlsym(RTLD_DEFAULT,"sceUserServiceGetInitialUser"));
            users_=reinterpret_cast<Users>(dlsym(RTLD_DEFAULT,"sceUserServiceGetLoginUserIdList"));
            get_=reinterpret_cast<Get>(dlsym(RTLD_DEFAULT,"scePadGetHandle"));
            open_=reinterpret_cast<Open>(dlsym(RTLD_DEFAULT,"scePadOpen"));
            read_=reinterpret_cast<Read>(dlsym(RTLD_DEFAULT,"scePadReadState"));
            close_=reinterpret_cast<Close>(dlsym(RTLD_DEFAULT,"scePadClose"));
            resolved_=(get_ && read_ && (initial_ || users_));
        }
        // Keep controller 1 set by UI-kit observation; no overwrites of its
        // status or reads from its live input queue.
        for(unsigned slot=1;slot<4;++slot){
            state[slot]=-1;
            battery[slot]=-1;
            charging[slot]=-1;
        }
        if(!resolved_)return;
        std::int32_t initial_user=-1;
        if(initial_ && initial_(&initial_user)!=0)initial_user=-1;
        std::array<std::int32_t,4> logged{{-1,-1,-1,-1}};
        if(users_ && users_(logged.data())!=0)logged.fill(-1);
        if(initial_user<0){
            for(const auto user:logged)if(user>=0){initial_user=user;break;}
        }
        if(initial_user<0)return;

        // scePadGetHandle(user, *type*, index): type 0 is a standard pad.
        // Passing slot as type (the proposed port-based snippet) is incorrect.
        // Additional indices may be unsupported on particular PS5 builds;
        // leave those slots unknown rather than fabricate disconnected pads.
        for(int index=1;index<4;++index){
            const int status=read_pad(initial_user,0,index);
            if(status>=0)state[static_cast<unsigned>(index)]=status;
        }
        // Other signed-in users can own additional pads. Place confirmed
        // connected pads in remaining empty slots without touching the UI pad.
        for(const auto user:logged){
            if(user<0 || user==initial_user)continue;
            const int status=read_pad(user,0,0);
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
    int read_pad(std::int32_t user,int type,int index){
        int handle=get_(user,type,index);
        bool owned=false;
        if(handle<0 && open_ && close_){
            handle=open_(user,type,index,nullptr);
            owned=handle>=0;
        }
        if(handle<0)return -1;
        alignas(8) unsigned char sample[120]{};
        const int result=read_(handle,sample);
        if(owned)close_(handle);
        if(result!=0)return -1;
        std::int32_t connected=0;
        std::memcpy(&connected,sample+0x4c,sizeof(connected));
        return connected!=0?1:0;
    }
#endif
};
}
