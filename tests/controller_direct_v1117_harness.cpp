#include "controller_status.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>

static int open_count=0,close_count=0;
static bool second_present=true;
extern "C" {
int sceUserServiceGetInitialUser(std::int32_t* p){*p=42;return 0;}
int sceUserServiceGetLoginUserIdList(std::int32_t* p){p[0]=42;p[1]=-1;p[2]=-1;p[3]=-1;return 0;}
int scePadGetHandle(std::int32_t,int,int){return -1;}
int scePadOpen(std::int32_t u,int type,int index,const void*){
    if(u==42 && type==0 && index==1 && second_present){++open_count;return 101;}
    return -3;
}
int scePadReadState(int handle,void* data){
    if(handle!=101)return -4;
    std::int32_t conn=1;
    std::memcpy(static_cast<unsigned char*>(data)+0x4c,&conn,4);
    return 0;
}
int scePadClose(int h){assert(h==101);++close_count;return 0;}
}
int main(){
    ulg::ControllerStatus m;
    m.observe_primary(true);
    m.poll();
    assert(m.symbols_ready && m.poll_count==1);
    assert(m.state[0]==1 && m.state[1]==1 && m.state[2]==-1);
    assert(m.last_get[1]==-1 && m.last_open[1]==101 && m.last_read[1]==0);
    assert(m.last_initial_user==42 && m.last_login_users[0]==42);
    assert(open_count==1 && close_count==1);
    second_present=false;
    m.poll();
    assert(m.state[0]==1 && m.state[1]==-1);
    assert(m.last_get[1]==-1 && m.last_open[1]<0 && m.last_read[1]==-999);
    assert(close_count==1);
}
