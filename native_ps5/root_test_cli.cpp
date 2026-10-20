// Testable root settings, browser and POSIX errno diagnostic without a PS5.
#include "root_manager.hpp"
#include "library_core.hpp"
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>
int main(int argc,char **argv) {
    if(argc!=4){std::cerr<<"usage: roots-test-cli <config> <save-file> <directory>\n";return 2;}
    std::vector<ulg::Root> roots{{"PS2",argv[3]}, {"Switch","/not-existing-test-ps5-launcher-root",false}};
    if(!ulg::save_user_roots(roots,argv[2]))return 3;
    const auto copy=ulg::configured_roots(argv[2],argv[1]);
    if(copy.size()!=2||!copy[0].enabled||copy[1].enabled)return 4;
    const auto snapshot=ulg::scan(copy);
    if(snapshot.failed_operations || snapshot.disabled_roots!=1 || snapshot.roots.size()!=2)return 5;
    const auto listing=ulg::browse_folders(argv[3]);
    if(listing.state.status!="ok")return 6;
    const auto missing=ulg::probe_root({"PS2","/not-existing-test-ps5-launcher-root"});
    if(missing.error_code!=ENOENT || missing.operation!="lstat" || missing.status!="offline")return 7;
    const auto not_dir=ulg::probe_root({"PS2",argv[1]});
    if(not_dir.error_code!=ENOTDIR || not_dir.status!="kein_ordner")return 8;
    std::cout<<"OK root-settings, browse, errno diagnostics\n";
    return 0;
}
