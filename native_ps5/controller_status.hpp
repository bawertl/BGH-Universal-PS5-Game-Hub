// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#if defined(BGH_TARGET_PS5)
#include <dlfcn.h>
#endif
namespace ulg {
// ReadState does not drain the UI kit's buffered input. Only handles opened
// by this monitor are closed; the UI controller remains owned by the UI kit.
class ControllerStatus {
public:
    // -1 unknown (API/read failed), 0 disconnected, 1 connected.
    std::array<int,4> state{{-1,-1,-1,-1}};
    // -1 means unknown: PS5 scePadReadState does not expose a verified battery
    // level or charging byte in the public 120-byte sample. Do not use 0x54.
    std::array<int,4> battery{{-1,-1,-1,-1}};
    // -1 unknown, 0 not charging, 1 charging (currently never guessed).
    std::array<int,4> charging{{-1,-1,-1,-1}};
    ControllerStatus()=default;
    // The UI kit already reads the initial user's real DualSense every frame.
    // This is authoritative for slot 1 even if optional monitor symbols are missing.
    void observe_primary(bool connected){
        state[0]=connected?1:0;
        if(!connected){battery[0]=-1;charging[0]=-1;}
    }
    ControllerStatus(const ControllerStatus&)=delete;
    ControllerStatus& operator=(const ControllerStatus&)=delete;
    ~ControllerStatus(){for(unsigned i=0;i<4;++i)release(i);}
    void poll(){
#if defined(BGH_TARGET_PS5)
        if(!resolved_){
            users_fn_=reinterpret_cast<Users>(dlsym(RTLD_DEFAULT,"sceUserServiceGetLoginUserIdList"));
            get_=reinterpret_cast<Get>(dlsym(RTLD_DEFAULT,"scePadGetHandle"));
            open_=reinterpret_cast<Open>(dlsym(RTLD_DEFAULT,"scePadOpen"));
            read_=reinterpret_cast<Read>(dlsym(RTLD_DEFAULT,"scePadReadState"));
            close_=reinterpret_cast<Close>(dlsym(RTLD_DEFAULT,"scePadClose"));
            // Retry discovery on later polls if PS5 runtime symbols are not yet present.
            resolved_=users_fn_ && get_ && open_ && read_ && close_;
        }
        state.fill(-1);
        battery.fill(-1);
        charging.fill(-1);
        if(!users_fn_ || !get_ || !open_ || !read_ || !close_)return;
        std::array<std::int32_t,4> users{{-1,-1,-1,-1}};
        if(users_fn_(users.data())!=0)return;
        for(unsigned i=0;i<4;++i){
            if(users[i]!=users_[i]){release(i);users_[i]=users[i];}
            if(users[i]==-1){state[i]=0;continue;}
            if(handles_[i]<0){
                handles_[i]=get_(users[i],0,0);
                if(handles_[i]<0){handles_[i]=open_(users[i],0,0,nullptr);owned_[i]=handles_[i]>=0;}
            }
            if(handles_[i]<0)continue;
            // 120-byte scePad sample, connected int32 at 0x4c, verified in
            // the pinned ps5-homebrew-ui pad.cpp and ProsperoEden ps5_pad.hpp.
            alignas(8) unsigned char sample[120]{};
            if(read_(handles_[i],sample)!=0){release(i);continue;}
            std::int32_t connected=0;
            std::memcpy(&connected,sample+0x4c,sizeof(connected));
            state[i]=connected!=0?1:0;
        }
#endif
    }
private:
    void release(unsigned i){
#if defined(BGH_TARGET_PS5)
        if(owned_[i] && handles_[i]>=0 && close_)close_(handles_[i]);
        handles_[i]=-1;owned_[i]=false;
#else
        (void)i;
#endif
    }
#if defined(BGH_TARGET_PS5)
    using Users=int(*)(std::int32_t*);
    using Get=int(*)(std::int32_t,int,int);
    using Open=int(*)(std::int32_t,int,int,const void*);
    using Read=int(*)(int,void*);
    using Close=int(*)(int);
    Users users_fn_=nullptr;Get get_=nullptr;Open open_=nullptr;
    Read read_=nullptr;Close close_=nullptr;bool resolved_=false;
    std::array<std::int32_t,4> users_{{-1,-1,-1,-1}};
    std::array<int,4> handles_{{-1,-1,-1,-1}};
    std::array<bool,4> owned_{};
#endif
};
}
