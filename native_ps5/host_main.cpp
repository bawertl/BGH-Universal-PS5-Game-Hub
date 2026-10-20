// SPDX-License-Identifier: MIT
#include "library_core.hpp"
#include <iostream>
#include <string>
int main(int argc,char **argv){
    std::string paths="assets/paths.conf";std::string output;std::string covers;
    for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--paths"&&i+1<argc)paths=argv[++i];else if(a=="--output"&&i+1<argc)output=argv[++i];else if(a=="--covers"&&i+1<argc)covers=argv[++i];}
    auto catalogue=ulg::scan(ulg::load_roots(paths));
    if(!covers.empty())ulg::apply_cover_overrides(catalogue,covers);
    if(!output.empty() && !ulg::write_json(catalogue,output)){std::cerr<<"JSON-Ausgabe fehlgeschlagen\n";return 2;}
    std::cout<<"Spiele: "<<catalogue.games.size()<<" | Wurzeln: "<<catalogue.roots.size()<<" | Fehler: "<<catalogue.errors.size()<<"\n";
    return 0;
}
