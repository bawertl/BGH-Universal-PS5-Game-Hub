// BGH V0.8 host harness. Does not need PS5 SDK or an emulator.
#include "emulator_covers.hpp"
#include <iostream>
#include <string>
int main(int argc,char **argv){
    if(argc==3 && std::string(argv[1])=="hash"){
        std::cout<<ulg::eden_cover_filename(argv[2])<<"\n";return 0;
    }
    if((argc!=10 && argc!=11) || std::string(argv[1])!="import")return 2;
    ulg::EmulatorCoverSources sources;
    sources.ps2_manual=argv[6];sources.ps2_cache=argv[6];
    sources.ps2_serial_cache=std::string(argv[6])+"/chd-serials.txt";
    sources.switch_covers=argv[6];sources.xbox_user_covers=argv[6];sources.xbox_covers=argv[6];
    sources.xbox_installed_covers=argv[6];
    ulg::Game game;game.platform=argv[2];game.path=argv[3];game.id=argv[4];
    game.title_id=argv[5];game.cover_path=argv[9];
    if(argc==11)game.format=argv[10];
    ulg::EmulatorCoverImporter importer(argv[7],std::stoi(argv[8]),sources);
    std::string diagnostic;
    const bool result=importer.hydrate(game,&diagnostic);
    std::cout<<(result?"FOUND":"SKIP")<<"|"<<game.cover_path<<"|"<<diagnostic<<"\n";
    return game.cover_path.empty()?1:0;
}
