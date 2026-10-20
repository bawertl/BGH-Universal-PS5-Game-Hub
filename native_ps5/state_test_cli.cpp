// Native and host test for on-disk catalogue cache, without accessing console storage.
#include "library_core.hpp"
#include "state_store.hpp"
#include <iostream>
int main(int argc, char **argv) {
    if(argc!=4){std::cerr<<"usage: state-test-cli ROOT_CONFIG CACHE PREFS\n";return 2;}
    const auto scanned=ulg::scan(ulg::load_roots(argv[1]));
    if(!ulg::write_catalogue_cache(scanned,argv[2]))return 3;
    ulg::Catalogue restored;if(!ulg::read_catalogue_cache(argv[2],restored))return 4;
    if(restored.games.size()!=scanned.games.size())return 5;
    ulg::Preferences pref;for(const auto &g:restored.games){pref.favorites.insert(g.id);pref.recent.push_back(g.id);}
    if(!ulg::write_preferences(pref,argv[3]))return 6;
    ulg::Preferences loaded;if(!ulg::read_preferences(argv[3],loaded))return 7;
    if(loaded.favorites!=pref.favorites || loaded.recent!=pref.recent)return 8;
    std::cout<<"OK Spiele="<<restored.games.size()<<"\n";
    return 0;
}
