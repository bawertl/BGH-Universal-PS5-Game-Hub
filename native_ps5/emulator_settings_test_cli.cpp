// Host-only persistence regression for emulator boot choice (no PS5 calls).
#include "state_store.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

static std::string file_bytes(const std::string &path){
    std::ifstream in(path,std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    const std::string root=argv[1];
    const std::string legacy=root+"/preferences.bin";
    const std::string primary=root+"/emulator_primary.bin";
    const std::string backup=root+"/emulator_fallback.bin";
    ulg::EmulatorBootSettings defaults{};
    if(!defaults.direct_boot || defaults.revision!=0)return 3;
    ulg::Preferences existing;
    existing.favorites={"id-for-ps5","id-for-switch"};
    existing.recent={"recent-a","recent-b","recent-c"};
    if(!ulg::write_preferences(existing,legacy))return 4;
    const std::string original=file_bytes(legacy);
    if(original.size()<12 || original.substr(0,4)!="ULGP")return 5;
    // A separate ULGE file must NEVER rewrite/migrate the working ULGP file.
    if(!ulg::write_emulator_boot_settings({false,1},primary))return 6;
    if(!ulg::write_emulator_boot_settings({true,2},backup))return 7;
    ulg::EmulatorBootSettings a{},b{};
    if(!ulg::read_emulator_boot_settings(primary,a) || a.direct_boot || a.revision!=1)return 8;
    if(!ulg::read_emulator_boot_settings(backup,b) || !b.direct_boot || b.revision!=2)return 9;
    if(file_bytes(legacy)!=original)return 10;
    ulg::Preferences restored;
    if(!ulg::read_preferences(legacy,restored) || restored.favorites!=existing.favorites ||
       restored.recent!=existing.recent)return 11;
    // Newer replica wins even when old primary storage is still readable.
    if(!(b.revision>a.revision))return 12;
    // Changing one replica never modifies the previous revision or its checksum.
    if(!ulg::write_emulator_boot_settings({false,3},primary))return 13;
    if(!ulg::read_emulator_boot_settings(primary,a) || a.direct_boot || a.revision!=3)return 14;
    if(file_bytes(legacy)!=original)return 15;
    // Corrupt checksums must not overwrite a previously valid caller value.
    auto raw=file_bytes(primary);raw[5]^=0x40;
    {
        std::ofstream out(primary,std::ios::binary|std::ios::trunc);
        out.write(raw.data(),static_cast<std::streamsize>(raw.size()));
    }
    ulg::EmulatorBootSettings unchanged{false,42};
    ulg::StorageStatus status;
    if(ulg::read_emulator_boot_settings(primary,unchanged,&status) || status.stage!="checksum" ||
       unchanged.revision!=42 || unchanged.direct_boot)return 16;
    if(ulg::read_emulator_boot_settings(root+"/missing.bin",unchanged) ||
       unchanged.revision!=42)return 17;
    if(ulg::write_emulator_boot_settings({false,4},root+"/not_found/settings.bin"))return 18;
    std::cout<<"OK ULGP-preserved ULGE-versioned checksum-rejected"<<std::endl;
    return 0;
}
