// Local test runner only. Does not initiate any launch without command-line arguments.
// SPDX-License-Identifier: MIT
#include "launcher_client.hpp"
#include <iostream>
#include <string>
int main(int argc, char **argv) {
    if(argc != 4) {std::cerr<<"usage: launch-test-cli PLATFORM PATH PORT\n";return 2;}
    const std::string platform=argv[1];
    ulg::LaunchPlan p=platform=="PS5" ? ulg::make_registered_ps5_launch_plan(argv[2])
        : platform=="PS4" ? ulg::make_registered_ps4_launch_plan(argv[2])
        : platform=="PS2MENU" ? ulg::make_ps5sx2_menu_launch_plan()
        : platform=="SWITCHMENU" ? ulg::make_prosperoeden_menu_launch_plan()
        : platform=="XBOXMENU" ? ulg::make_ps5x360_menu_launch_plan()
        : ulg::make_launch_plan(platform,argv[2]);
    auto result=ulg::send_launch(p,static_cast<unsigned short>(std::stoi(argv[3])));
    std::cout<<static_cast<int>(result.status)<<"|"<<result.message<<"\n";
    return 0;
}
