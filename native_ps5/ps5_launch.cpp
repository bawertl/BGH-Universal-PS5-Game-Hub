// SPDX-License-Identifier: MIT
#include "ps5_launch.hpp"
#include <cstdint>
#include <cstdio>
namespace ulg {
bool valid_native_ps5_title_id(const std::string& platform,const std::string& id){
    if(platform!="PS5" || id.size()!=9 || id.compare(0,4,"PPSA")!=0)return false;
    if(id=="PPSA99079" || id=="PPSA99008" || id=="PPSA99203" || id=="PPSA50011")return false;
    for(int i=4;i<9;++i)if(id[static_cast<std::size_t>(i)]<'0'||id[static_cast<std::size_t>(i)]>'9')return false;
    return true;
}
#if defined(BGH_TARGET_PS5)
extern "C" {
    struct AppLaunchContext {
        std::uint32_t structsize;
        std::uint32_t user_id;
        std::uint32_t app_opt;
        std::uint64_t crash_report;
        std::uint64_t check_flag;
    };
    int sceUserServiceGetForegroundUser(std::uint32_t* user_id);
    int sceSystemServiceLaunchApp(const char* title_id,const char** argv,AppLaunchContext* ctx);
}
#endif
static NativePs5LaunchResult launch_title(const std::string& title_id){
    NativePs5LaunchResult result;
#if defined(BGH_TARGET_PS5)
    AppLaunchContext ctx{};
    ctx.structsize=sizeof(ctx);
    int rc=sceUserServiceGetForegroundUser(&ctx.user_id);
    if(rc!=0){result.result_code=rc;result.message="Kein aktiver PS5-Benutzer";return result;}
    const char* args[]={nullptr};
    rc=sceSystemServiceLaunchApp(title_id.c_str(),args,&ctx);
    result.result_code=rc;
    if(rc==0){result.accepted=true;result.message="PS5-Spielstart an System uebergeben";}
    else{
        char message[130];
        std::snprintf(message,sizeof(message),"Start %s abgelehnt: 0x%08X (Ursache nicht bestaetigt)",title_id.c_str(),static_cast<unsigned>(rc));
        result.message=message;
    }
#else
    (void)title_id;
    result.message="Nur auf PS5-Hardware verfuegbar";
#endif
    return result;
}
NativePs5LaunchResult launch_registered_ps5(const std::string& title_id){
    if(!valid_native_ps5_title_id("PS5",title_id))
        return {false,0,"Kein gueltiger PS5-Spieltitel (PPSA-ID)"};
    return launch_title(title_id);
}
NativePs5LaunchResult launch_emulator_menu(const std::string& platform){
    const char* id=platform=="PS2"?"PPSA99203":platform=="Switch"?"PPSA99008":
        platform=="Xbox 360"?"PPSA50011":nullptr;
    if(!id)return {false,0,"Kein bekannter Emulator fuer diese Plattform"};
    auto result=launch_title(id);
    if(result.accepted)result.message="Emulatorstart uebergeben; Spiel im Emulator auswaehlen";
    return result;
}
} // namespace ulg
