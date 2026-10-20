#include "ftp_readonly.hpp"
#include "ftp_scan.hpp"
#include <cstdlib>
#include <iostream>
int main(int argc,char **argv){
    if(argc!=3)return 2;
    const int port=std::atoi(argv[1]);
    {
        ulg::FtpReadonly ftp(port);
        if(!ftp.connect_local())return 3;
        const auto listing=ftp.list(argv[2]);
        if(!listing.ok)return 4;
        std::cout<<"MLSD entries="<<listing.entries.size()<<"\n";
        bool got_game=false;
        for(const auto &entry:listing.entries)if(entry.name=="Example.xci")got_game=true;
        if(!got_game)return 5;
    }
    ulg::Catalogue cat=ulg::scan({{"Switch",argv[2]}});
    ulg::supplement_from_local_ftp(cat,{{"Switch",argv[2]}},5,5000,port);
    if(cat.games.size()!=1 || !cat.games[0].via_ftp || cat.roots[0].status!="ftp_ok" || cat.failed_roots!=0){
        std::cerr<<"bad: games="<<cat.games.size()<<" root="<<cat.roots[0].status<<" failed="<<cat.failed_roots<<" total="<<cat.failed_operations<<"\n";
        return 6;
    }
    std::cout<<"Recovered game: "<<cat.games[0].title<<"\n";
    return 0;
}
