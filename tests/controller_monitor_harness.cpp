#define BGH_TARGET_PS5
#include "controller_status.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
int closed=0;
extern "C" int sceUserServiceGetLoginUserIdList(std::int32_t* u){u[0]=10;u[1]=20;u[2]=-1;u[3]=30;return 0;}
extern "C" int scePadGetHandle(std::int32_t u,int,int){return u==10?100:-1;}
extern "C" int scePadOpen(std::int32_t u,int,int,const void*){return u==20?200:-1;}
extern "C" int scePadClose(int h){assert(h==200);++closed;return 0;}
extern "C" int scePadReadState(int h,void* p){std::memset(p,0,120);int c=h==100?1:0;std::memcpy(static_cast<char*>(p)+0x4c,&c,4);return 0;}
int main(){
 {ulg::ControllerStatus m;m.poll();assert(m.state[0]==1 && m.state[1]==0 && m.state[2]==0 && m.state[3]==-1);m.poll();assert(closed==0);m.observe_primary(true);assert(m.state[0]==1);m.observe_primary(false);assert(m.state[0]==0);assert(m.state[1]==0);}
 assert(closed==1);
}
