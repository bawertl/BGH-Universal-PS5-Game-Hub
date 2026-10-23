// BGH: automatically expose configured emulator applications in the Emulators tab.
// Synthetic entries are view-only; no changes to user games or their locations.
// SPDX-License-Identifier: MIT
#pragma once
#include "emulator_detection.hpp"
#include <algorithm>
#include <map>
#include <string>

namespace ulg {
namespace emulator_cards {
inline constexpr const char *kFormat="emulator_kachel";
inline bool is_auto_card(const Game &g){
    return g.platform=="Homebrew" && g.format==kFormat;
}
inline bool matches_installed(const Game &g,const std::string &title_id){
    return g.platform=="Homebrew" && g.title_id==title_id;
}
// No directory traversal or app launches here: detection has already validated
// metadata/required files, using POSIX or read-only local FTP.
inline void sync(Catalogue &catalogue,const PlatformRegistry &registry,
                 const std::map<std::string,emulator_detection::Detection> &detected){
    auto &games=catalogue.games;
    games.erase(std::remove_if(games.begin(),games.end(),is_auto_card),games.end());
    for(const auto &platform:registry.entries){
        if(!platform.enabled || !platform.is_emulator || platform.app_path.empty())continue;
        const auto found=detected.find(platform.id);
        if(found==detected.end())continue;
        const auto &d=found->second;
        const bool known=d.status==emulator_detection::Detection::Status::known_emulator;
        const bool configured_app=d.status==emulator_detection::Detection::Status::unidentified_app;
        if((!known && !configured_app) || !emulator_detection::ps5_title_id(d.title_id))continue;
        const bool already=std::any_of(games.begin(),games.end(),[&](const Game &g){
            return matches_installed(g,d.title_id);
        });
        if(already)continue;
        Game game;
        game.platform="Homebrew";
        game.format=kFormat;
        game.id="emulator-app-"+d.title_id;
        game.title=d.name.empty()?d.title_id:d.name;
        if(configured_app)game.title+=" (Profil fehlt)";
        game.title_id=d.title_id;
        game.path=platform.app_path;
        game.available=true;
        game.via_ftp=d.via_ftp;
        games.push_back(std::move(game));
    }
}
} // namespace emulator_cards
} // namespace ulg
