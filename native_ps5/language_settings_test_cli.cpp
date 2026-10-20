// Host-only ULGL compatibility and localization regression; no console APIs.
#include "state_store.hpp"
#include "ui_localization.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
static std::string read_all(const std::string &path){
    std::ifstream in(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    const std::string root=argv[1];
    const auto prefs=root+"/preferences.bin";
    const auto boot=root+"/emulator_boot.bin";
    const auto primary=root+"/language_primary.bin";
    const auto secondary=root+"/language_secondary.bin";
    ulg::Preferences p;p.favorites={"ID_A"};p.recent={"PATH|PS5|/user/app/A","PATH|Switch|/mnt/usb0/B"};
    if(!ulg::write_preferences(p,prefs))return 3;
    if(!ulg::write_emulator_boot_settings({false,7},boot))return 4;
    const auto prefs_original=read_all(prefs),boot_original=read_all(boot);
    ulg::LanguageSettings def;
    if(def.english || def.revision!=0)return 5;
    if(!ulg::write_language_settings({true,1},primary) ||
       !ulg::write_language_settings({false,2},secondary))return 6;
    ulg::LanguageSettings a{},b{};
    if(!ulg::read_language_settings(primary,a) || !a.english || a.revision!=1)return 7;
    if(!ulg::read_language_settings(secondary,b) || b.english || b.revision!=2)return 8;
    if(!(b.revision>a.revision))return 9;
    if(read_all(prefs)!=prefs_original || read_all(boot)!=boot_original)return 10;
    if(!ulg::write_language_settings({true,3},primary))return 11;
    std::string raw=read_all(primary);
    if(raw.size()!=20 || raw.substr(0,4)!="ULGL")return 12;
    raw[7]^=1;
    {std::ofstream out(primary,std::ios::binary|std::ios::trunc);
     out.write(raw.data(),static_cast<std::streamsize>(raw.size()));}
    ulg::LanguageSettings keep{false,44};ulg::StorageStatus status;
    if(ulg::read_language_settings(primary,keep,&status) ||
       status.stage!="checksum" || keep.english || keep.revision!=44)return 13;
    if(ulg::read_language_settings(root+"/missing",keep) || keep.revision!=44)return 14;
    if(!ulg::write_language_settings({true,4},primary))return 15;
    raw=read_all(primary);raw[12]=2; // invalid language encoding, recompute FNV to test field validation
    std::uint32_t hash=2166136261u;
    for(std::size_t i=0;i<raw.size()-4;++i){hash^=static_cast<unsigned char>(raw[i]);hash*=16777619u;}
    for(int i=0;i<4;++i)raw[raw.size()-4+i]=static_cast<char>(hash>>(i*8));
    {std::ofstream out(primary,std::ios::binary|std::ios::trunc);
     out.write(raw.data(),static_cast<std::streamsize>(raw.size()));}
    if(ulg::read_language_settings(primary,keep,&status) || status.stage!="invalid_content")return 16;
    if(ulg::translate_ui("Meine Spiele",true)!="My Games")return 17;
    if(ulg::translate_ui("Meine Spiele",false)!="Meine Spiele")return 18;
    if(ulg::translate_ui("Luigi's Mansion 3 [0100]",true)!="Luigi's Mansion 3 [0100]")return 19;
    if(std::string(ulg::tr_hint("Spielen",true))!="Play")return 20;
    if(read_all(prefs)!=prefs_original || read_all(boot)!=boot_original)return 21;
    std::cout<<"OK ULGL language saved, ULGP/ULGE unchanged, checksums checked\n";
    return 0;
}
