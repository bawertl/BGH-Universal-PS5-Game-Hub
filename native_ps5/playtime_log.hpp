// BGH V1.1.8: experimental read-only ShadowMountPlus log observation.
// Not a historical/active playtime counter; no writes and no game launch changes.
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <utility>

namespace ulg::playtime {
constexpr std::size_t kMaxLogBytes=2*1024*1024;

struct TitleSeconds {
    std::uint64_t seconds=0;
    unsigned sessions=0;
    unsigned unfinished=0;
    unsigned uncertain=0;
};
struct Summary {
    bool readable=false;
    bool has_events=false;
    unsigned clock_resets=0;
    unsigned duplicate_starts=0;
    unsigned orphan_ends=0;
    unsigned sleep_events=0;
    std::map<std::string,TitleSeconds> by_title;
};

inline bool read_local_log(const char* path,std::string& contents){
    contents.clear();
    FILE* f=std::fopen(path,"rb");
    if(!f)return false;
    if(std::fseek(f,0,SEEK_END)!=0){std::fclose(f);return false;}
    const long n=std::ftell(f);
    if(n<0 || static_cast<std::uint64_t>(n)>kMaxLogBytes){std::fclose(f);return false;}
    if(std::fseek(f,0,SEEK_SET)!=0){std::fclose(f);return false;}
    contents.resize(static_cast<std::size_t>(n));
    const std::size_t got=contents.empty()?0:std::fread(contents.data(),1,contents.size(),f);
    const bool good=!std::ferror(f);
    std::fclose(f);
    if(!good){contents.clear();return false;}
    contents.resize(got);return true;
}

inline bool valid_id(const std::string& id){
    if(id.size()!=9 || (id.compare(0,4,"PPSA")!=0 && id.compare(0,4,"CUSA")!=0))return false;
    for(std::size_t i=4;i<9;++i)if(id[i]<'0'||id[i]>'9')return false;
    return true;
}

// A PPSA/CUSA application ID can identify an individual *native* title.
// Emulator applications only identify their processes, not a ROM.
inline bool is_native_game_id(const std::string& platform,const std::string& id){
    if(!valid_id(id))return false;
    if(platform=="PS5")return id.compare(0,4,"PPSA")==0 &&
        id!="PPSA99203" && id!="PPSA99008" && id!="PPSA50011" && id!="PPSA99079";
    if(platform=="PS4")return id.compare(0,4,"CUSA")==0;
    return false;
}

inline bool event_pair(const std::string& line,const char* marker,std::string& title,std::string& pid){
    const auto at=line.find(marker);
    if(at==std::string::npos)return false;
    const std::size_t begin=at+std::char_traits<char>::length(marker);
    if(begin+9>=line.size())return false;
    title=line.substr(begin,9);
    if(!valid_id(title))return false;
    if(line.compare(begin+9,5," pid=")!=0)return false;
    const std::size_t first=begin+14;
    std::size_t end=first;
    while(end<line.size() && end-first<10 && line[end]>='0' && line[end]<='9')++end;
    if(end==first)return false;
    pid=line.substr(first,end-first);
    return true;
}

inline int clock_seconds(const std::string& line){
    if(line.size()<11 || line[0]!='[' || line[3]!=':' || line[6]!=':' || line[9]!=']')return -1;
    for(int i:{1,2,4,5,7,8})if(line[static_cast<std::size_t>(i)]<'0' || line[static_cast<std::size_t>(i)]>'9')return -1;
    const int h=(line[1]-'0')*10+line[2]-'0';
    const int m=(line[4]-'0')*10+line[5]-'0';
    const int s=(line[7]-'0')*10+line[8]-'0';
    if(h>23||m>59||s>59)return -1;
    return h*3600+m*60+s;
}

struct Run {
    std::string title;
    int epoch=0;
    std::int64_t started=0;
    std::uint64_t elapsed=0;
    bool running=false;
    bool uncertain=false;
};

inline Summary analyze(const std::string& text){
    Summary out;out.readable=true;
    std::map<std::string,Run> active;
    bool sleeping=false;
    int epoch=0,day=0,last=-1;
    std::size_t pos=0;
    unsigned lines=0;
    while(pos<text.size() && ++lines<=100000){
        auto eol=text.find('\n',pos);
        if(eol==std::string::npos)eol=text.size();
        const std::string line=text.substr(pos,eol-pos);
        pos=eol<text.size()?eol+1:eol;
        const int sec=clock_seconds(line);
        if(sec<0)continue;
        if(last>=0 && sec<last){
            if(last>=22*3600 && sec<=2*3600){++day;}
            else{
                ++epoch;day=0;++out.clock_resets;
                // A clock reset during an active process invalidates its whole
                // session; a reset in standby must not invalidate the running parts.
                if(!sleeping)for(auto& item:active){
                    item.second.uncertain=true;
                    item.second.running=false;
                }
            }
        }
        last=sec;
        const std::int64_t now=static_cast<std::int64_t>(day)*86400+sec;
        const auto stop_segment=[&](Run& r){
            if(!r.running)return;
            if(r.epoch!=epoch || now<r.started){r.uncertain=true;}
            else r.elapsed+=static_cast<std::uint64_t>(now-r.started);
            r.running=false;
        };
        if(line.find("[SLEEP] entered")!=std::string::npos){
            if(!sleeping){sleeping=true;++out.sleep_events;for(auto& item:active)stop_segment(item.second);}
            continue;
        }
        if(line.find("[SLEEP] left")!=std::string::npos){
            if(sleeping){sleeping=false;for(auto& item:active){
                if(!item.second.uncertain){item.second.running=true;item.second.epoch=epoch;item.second.started=now;}
            }}
            continue;
        }
        std::string title,pid;
        if(event_pair(line,"[GAME] started: ",title,pid)){
            out.has_events=true;
            const auto key=title+":"+pid;
            if(active.find(key)!=active.end()){++out.duplicate_starts;continue;}
            Run r;r.title=title;r.running=!sleeping;r.epoch=epoch;r.started=now;
            if(sleeping)r.uncertain=true;
            active.emplace(key,std::move(r));
        }else if(event_pair(line,"[GAME] finalizing stopped application: ",title,pid)){
            out.has_events=true;
            const auto key=title+":"+pid;
            auto it=active.find(key);
            if(it==active.end()){++out.orphan_ends;continue;}
            Run& r=it->second;
            stop_segment(r);
            auto& t=out.by_title[title];
            if(sleeping || r.uncertain){++t.uncertain;}
            else{t.seconds+=r.elapsed;++t.sessions;}
            active.erase(it);
        }
    }
    for(const auto& item:active)++out.by_title[item.second.title].unfinished;
    return out;
}

inline std::string duration_label(std::uint64_t seconds){
    const auto h=seconds/3600;
    const auto m=(seconds%3600)/60;
    if(h>0)return std::to_string(h)+" Std. "+std::to_string(m)+" Min.";
    if(m>0)return std::to_string(m)+" Min. "+std::to_string(seconds%60)+" Sek.";
    return std::to_string(seconds)+" Sek.";
}
} // namespace ulg::playtime
