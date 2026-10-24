// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

// The V1.1.16 hardware diagnostic showed api=UNAVAILABLE on PS5 13.60.
// Resolve through explicit SDK import stubs rather than RTLD_DEFAULT.
// The UI kit continues to own controller 1; no extra reader may consume it.
#if defined(BGH_TARGET_PS5)
extern "C" {
int sceUserServiceGetInitialUser(std::int32_t *user_id);
int sceUserServiceGetLoginUserIdList(std::int32_t *ids);
int scePadGetHandle(std::int32_t user_id, int type, int index);
int scePadOpen(std::int32_t user_id, int type, int index, const void *params);
int scePadReadState(int handle, void *state);
int scePadClose(int handle);
}
#endif

namespace ulg {
class ControllerStatus {
public:
    // -1 = not established, 0 = explicitly disconnected, 1 = connected.
    std::array<int,4> state{{-1,-1,-1,-1}};
    std::array<int,4> battery{{-1,-1,-1,-1}};
    std::array<int,4> charging{{-1,-1,-1,-1}};
    std::array<int,4> last_get{{-999,-999,-999,-999}};
    std::array<int,4> last_open{{-999,-999,-999,-999}};
    std::array<int,4> last_read{{-999,-999,-999,-999}};
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
    std::string probe_summary(unsigned slot) const {
        if(slot>=4)return "?";
        return diagnostic_number(last_get[slot])+"/"+
            diagnostic_number(last_open[slot])+"/"+
            diagnostic_number(last_read[slot]);
    }
    std::string diagnostic_pad(unsigned slot) const {
        if(slot>=4)return "?";
        const char *status=state[slot]==1?"ON":(state[slot]==0?"OFF":"UNKNOWN");
        return "C"+std::to_string(slot+1)+" "+status+" G/O/R="+probe_summary(slot);
    }
    void observe_primary(bool connected){
        state[0]=connected?1:0;
        if(!connected){battery[0]=-1;charging[0]=-1;}
    }
    void poll(){
#if defined(BGH_TARGET_PS5)
        ++poll_count;
        symbols_ready=true; // Direct imports: linking must succeed or build fails.
        last_initial_result=-999;
        last_initial_user=-1;
        last_login_result=-999;
        last_login_users.fill(-1);
        // NEVER overwrite/borrow/close the UI kit's primary controller handle.
        for(unsigned i=1;i<4;++i){
            state[i]=-1;
            last_get[i]=last_open[i]=last_read[i]=-999;
            battery[i]=charging[i]=-1;
        }
        std::int32_t initial_user=-1;
        last_initial_result=sceUserServiceGetInitialUser(&initial_user);
        if(last_initial_result!=0)initial_user=-1;
        last_initial_user=initial_user;
        std::array<std::int32_t,4> users{{-1,-1,-1,-1}};
        last_login_result=sceUserServiceGetLoginUserIdList(users.data());
        if(last_login_result!=0)users.fill(-1);
        last_login_users=users;
        if(initial_user<0){
            for(const auto id:users)if(id>=0){initial_user=id;break;}
        }
        if(initial_user<0)return;
        // type=0 is standard gamepad. index 1-3 are secondary to primary
        // index 0. The original index loop remains testable via diagnostics.
        for(int index=1;index<4;++index){
            const int result=read_pad(initial_user,0,index,index);
            if(result>=0)state[static_cast<unsigned>(index)]=result;
        }
        // Pads owned by other logged-in users may use index 0. Never ask for
        // index 0 of the initial user's active pad, owned by the UI kit.
        for(unsigned position=0;position<users.size();++position){
            const auto user=users[position];
            if(user<0 || user==initial_user)continue;
            bool duplicate=false;
            for(unsigned prior=0;prior<position;++prior)
                if(users[prior]==user){duplicate=true;break;}
            if(duplicate)continue;
            int free_slot=-1;
            for(int slot=1;slot<4;++slot){
                if(state[static_cast<unsigned>(slot)]!=1){free_slot=slot;break;}
            }
            if(free_slot<0)break;
            const int result=read_pad(user,0,0,free_slot);
            if(result==1)state[static_cast<unsigned>(free_slot)]=1;
        }
#endif
    }
private:
#if defined(BGH_TARGET_PS5)
    int read_pad(std::int32_t user,int type,int index,int diagnostic_slot){
        const auto capture=[&](std::array<int,4> &field,int result){
            if(diagnostic_slot>=0 && diagnostic_slot<4)
                field[static_cast<unsigned>(diagnostic_slot)]=result;
        };
        int handle=scePadGetHandle(user,type,index);
        capture(last_get,handle);
        bool owned=false;
        if(handle<0){
            handle=scePadOpen(user,type,index,nullptr);
            capture(last_open,handle);
            owned=(handle>=0);
        }
        if(handle<0)return -1;
        // Current-state sample is 120 bytes; using exactly one state record.
        alignas(8) unsigned char sample[120]{};
        const int result=scePadReadState(handle,sample);
        capture(last_read,result);
        if(owned)(void)scePadClose(handle);
        if(result!=0)return -1;
        std::int32_t connected=0;
        std::memcpy(&connected,sample+0x4c,sizeof(connected));
        return connected!=0?1:0;
    }
#endif
};
} // namespace ulg
