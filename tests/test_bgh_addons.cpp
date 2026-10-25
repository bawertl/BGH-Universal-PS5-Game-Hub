#include "bgh_theme_gallery.hpp"
#include "bgh_storage_metrics.hpp"
#include "bgh_nextgen_layout.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
int main() {
    using namespace ulg::bgh_gallery;
    assert(ulg::bgh_layout::center_x==960.0f);
    assert(ulg::bgh_layout::card_center(3)==960.0f);
    assert(ulg::bgh_layout::text_x(200.f)==860.f);
    assert(ulg::bgh_layout::card_left(3) - (ulg::bgh_layout::card_center(2) + ulg::bgh_layout::card_width[2]/2)>20.f);
    assert(!valid_filename("../../game.png"));
    assert(!valid_filename(".secret.png"));
    assert(valid_filename("mein_hintergrund.JPG"));
    assert(!valid_filename("cover.png/../"));
    const auto all_backgrounds=builtins(Kind::background);
    assert(all_backgrounds.size()==9);
    for (const auto& item:all_backgrounds) {
        assert(!item.user_supplied);
        assert(!item.path.empty());
        assert(builtin_background_preview(item.id).find("/app0/assets/themes/backgrounds/previews/")==0);
    }
    assert(builtin_background_preview("unrecognized").empty());
    Selection selection;
    assert(!logo_should_render(Selection{"mountain_crown","crown"}));
    assert(logo_should_render(selection));
    assert(select(Kind::background,"mountain_crown",selection));
    assert(select(Kind::background,"orbital_horizon",selection));
    assert(selection.background=="orbital_horizon");
    assert(select(Kind::background,"mountain_crown",selection));
    assert(select(Kind::logo,"none",selection));
    assert(!logo_should_render(selection));
    const std::string tmp="/tmp/bgh_test_gallery_selection.conf";
    assert(save_selection(tmp,selection));
    Selection loaded;
    assert(load_selection(tmp,loaded));
    assert(loaded.background=="mountain_crown" && loaded.logo=="none");
    std::remove(tmp.c_str());
    assert(!remove_user_image(Kind::background,"lake",selection,true));
    assert(!remove_user_image(Kind::background,"user:../../etc/passwd",selection,true));
    assert(!remove_user_image(Kind::background,"user:image.png",selection,false));
    // PS5 13.60 startup guard: this helper must not touch statvfs at all.
    auto v = ulg::bgh_storage::probe("/tmp");
    assert(!v.known && v.capacity_bytes==0);
    assert(!ulg::bgh_storage::internal().known);
    assert(!ulg::bgh_storage::external().known);
    assert(ulg::bgh_storage::label(v)=="Nicht verfuegbar");
    return 0;
}
