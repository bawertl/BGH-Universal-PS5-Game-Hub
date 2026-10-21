#include "playtime_log.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
    using namespace ulg::playtime;
    const auto a=analyze("[12:00:00] [GAME] started: PPSA06084 pid=25\n[12:01:00] [GAME] started: PPSA06084 pid=25\n[12:02:00] [SLEEP] entered by x\n[15:00:00] [SLEEP] left by x\n[15:03:00] [GAME] finalizing stopped application: PPSA06084 pid=25\n");
    assert(a.by_title.at("PPSA06084").seconds==300 && a.by_title.at("PPSA06084").sessions==1 && a.duplicate_starts==1);
    const auto b=analyze("[23:59:00] [GAME] started: CUSA12345 pid=2\n[00:01:00] [GAME] finalizing stopped application: CUSA12345 pid=2\n");
    assert(b.by_title.at("CUSA12345").seconds==120);
    const auto c=analyze("[12:00:00] [GAME] started: PPSA06084 pid=2\n[11:59:00] [GAME] finalizing stopped application: PPSA06084 pid=2\n");
    assert(c.by_title.at("PPSA06084").uncertain==1 && c.by_title.at("PPSA06084").sessions==0);
    const auto d=analyze("[12:00:00] [GAME] started: PPSA06084 pid=2\n");
    assert(d.by_title.at("PPSA06084").unfinished==1 && d.by_title.at("PPSA06084").sessions==0);
    const auto e=analyze("[12:00:00] [GAME] started: PPSA06084 pid=204\n[12:01:00] [SLEEP] entered by x\n[11:00:00] [SLEEP] left by x\n[11:00:00] [GAME] started: PPSA06084 pid=204\n[11:01:27] [GAME] finalizing stopped application: PPSA06084 pid=204\n");
    assert(e.by_title.at("PPSA06084").seconds==147 && e.duplicate_starts==1);
    assert(!valid_id("PSAA06084") && !valid_id("PPSA0608"));
    if(argc==2){
        std::ifstream in(argv[1]);assert(in.good());
        const std::string text((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
        const auto actual=analyze(text);
        std::cout<<"events="<<actual.has_events<<" duplicates="<<actual.duplicate_starts<<" clock_resets="<<actual.clock_resets<<" titles="<<actual.by_title.size()<<"\n";
        for(const auto& item:actual.by_title)std::cout<<item.first<<" "<<item.second.sessions<<" "<<item.second.seconds<<" "<<item.second.uncertain<<" "<<item.second.unfinished<<"\n";
    }else std::cout<<"Playtime host tests OK\n";
}
