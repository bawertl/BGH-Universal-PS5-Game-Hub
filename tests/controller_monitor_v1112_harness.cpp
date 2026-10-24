#define BGH_TARGET_PS5
#include "controller_status.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
int open_calls=0,close_calls=0,read_calls=0,get_type_error=0;
extern "C" int sceUserServiceGetInitialUser(std::int32_t* user){*user=101;return 0;}
extern "C" int sceUserServiceGetLoginUserIdList(std::int32_t* users){users[0]=101;users[1]=-1;users[2]=-1;users[3]=-1;return 0;}
extern "C" int scePadGetHandle(std::int32_t user,int type,int index){
    if(type!=0){++get_type_error;return -1;}
    return (user==101 && index==1)?11:-1;
}
extern "C" int scePadOpen(std::int32_t user,int type,int index,const void*){
    ++open_calls;
    return (user==101 && type==0 && index==2)?22:-1;
}
extern "C" int scePadReadState(int handle,void* sample){
    ++read_calls;
    std::memset(sample,0,120);
    const std::int32_t value=(handle==11)?1:0;
    std::memcpy(static_cast<unsigned char*>(sample)+0x4c,&value,4);
    return 0;
}
extern "C" int scePadClose(int handle){assert(handle==22);++close_calls;return 0;}
int main(){
    ulg::ControllerStatus m;
    m.observe_primary(true);
    m.poll();
    assert(m.state[0]==1);
    assert(m.state[1]==1);
    assert(m.state[2]==0);
    assert(m.state[3]==-1);
    assert(m.battery[1]==-1);
    assert(get_type_error==0);
    assert(close_calls==1);
    assert(read_calls==2);
    m.observe_primary(false);
    assert(m.state[0]==0);
    // Borrowed UI-kit handle 11 must not be closed.
    assert(open_calls>=2);
}
