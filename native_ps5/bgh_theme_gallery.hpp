// SPDX-License-Identifier: MIT
#pragma once
// Optional BGH theme gallery backend. It DOES NOT modify existing theme.bin,
// games, emulator paths, or the UI until deliberately called by the renderer.
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// Test builds can redirect only the BGH-owned import cache to a temp folder.
#ifndef BGH_GALLERY_CACHE_ROOT
#define BGH_GALLERY_CACHE_ROOT "/download0/hui/ulg_v04/user_themes"
#endif
namespace ulg {
namespace bgh_gallery {
enum class Kind { background, logo };
struct ThemeItem {
    std::string id;
    std::string path;
    bool user_supplied = false;
    bool contains_logo = false;
};
struct Selection {
    std::string background = "lake";
    std::string logo = "legacy";
};
inline bool valid_filename(const std::string& filename) {
    if (filename.empty() || filename.size() > 96 || filename[0] == '.') return false;
    for (const unsigned char c : filename) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' || c == ' ')) return false;
    }
    const auto point = filename.find_last_of('.');
    if (point == std::string::npos) return false;
    std::string ext = filename.substr(point);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a'-'A')) : static_cast<char>(c);
    });
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png";
}
// FTP-writable directories inside the installed PPSA99079 title.
// The two previous paths remain read-only discovery fallbacks for older users.
// Copies fetched from the already-running local read-only FTP service reside
// in the title's writable private area, never under a game directory.
inline const char* imported_folder(Kind kind) {
    return kind == Kind::background ?
        BGH_GALLERY_CACHE_ROOT "/backgrounds" :
        BGH_GALLERY_CACHE_ROOT "/logos";
}
inline const char* upload_folder(Kind kind) {
    return kind == Kind::background ?
        "/data/homebrew/PPSA99079/user_themes/backgrounds" :
        "/data/homebrew/PPSA99079/user_themes/logos";
}
inline const char* app0_upload_folder(Kind kind) {
    return kind == Kind::background ? "/app0/user_themes/backgrounds" :
                                     "/app0/user_themes/logos";
}
inline const char* legacy_upload_folder(Kind kind) {
    return kind == Kind::background ? "/data/bgh/themes/backgrounds" :
                                     "/data/bgh/themes/logos";
}
inline std::vector<ThemeItem> builtins(Kind kind) {
    if (kind == Kind::background) return {
        {"lake", "/app0/assets/premium_lake_background.png", false, false},
        {"showroom", "/app0/assets/themes/backgrounds/cyber_showroom.png", false, false},
        {"mountain_crown", "/app0/assets/themes/backgrounds/bgh_berg_cyber_mit_kronenlogo.png", false, true},
    };
    return {
        {"legacy", "/app0/assets/bgh_logo.png", false, false},
        {"crown", "/app0/assets/themes/logos/bgh_kronenlogo_freigestellt.png", false, false},
        {"none", "", false, false},
    };
}
inline std::vector<ThemeItem> list(Kind kind) {
    std::vector<ThemeItem> result = builtins(kind);
    // Prefer the writable install folder, with /app0 and the old user folder
    // supported for existing selections. Duplicate filenames keep one ID.
    const char* folders[] = {imported_folder(kind), upload_folder(kind),
                             app0_upload_folder(kind), legacy_upload_folder(kind)};
    for (const char* dir : folders) {
        DIR* dp = ::opendir(dir);
        if (!dp) continue; // No directory is not an error and never blocks boot.
        while (dirent* entry = ::readdir(dp)) {
            const std::string filename(entry->d_name);
            if (!valid_filename(filename)) continue;
            const std::string id = "user:" + filename;
            if (std::any_of(result.begin(),result.end(),[&](const ThemeItem& t){
                    return t.id == id;
                })) continue;
            const std::string path = std::string(dir) + "/" + filename;
            struct stat st{};
            if (::lstat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
            result.push_back({id, path, true, false});
            if (result.size() >= 67) break; // 3 built-ins + up to 64 images
        }
        ::closedir(dp);
        if (result.size() >= 67) break;
    }
    return result;
}
inline bool find(Kind kind, const std::string& id, ThemeItem& result) {
    for (const auto& item : list(kind)) if (item.id == id) { result = item; return true; }
    return false;
}
inline bool select(Kind kind, const std::string& id, Selection& sel) {
    ThemeItem found;
    if (!find(kind,id,found)) return false;
    if (kind == Kind::background) sel.background = id;
    else sel.logo = id;
    return true;
}
inline bool save_selection(const std::string& path, const Selection& sel) {
    // Own small text config; leaves the existing BGH theme.bin binary format alone.
    ThemeItem ignored;
    if (!find(Kind::background, sel.background, ignored) ||
        !find(Kind::logo, sel.logo, ignored)) return false;
    const std::string temp = path + ".tmp";
    FILE* fp = std::fopen(temp.c_str(), "wb");
    if (!fp) return false;
    const std::string data = "BGH_THEME_V1\n" + sel.background + "\n" + sel.logo + "\n";
    const bool ok = std::fwrite(data.data(),1,data.size(),fp) == data.size() &&
                    std::fflush(fp) == 0;
    const bool closed = std::fclose(fp) == 0;
    if (!ok || !closed || std::rename(temp.c_str(),path.c_str()) != 0) {
        std::remove(temp.c_str()); return false;
    }
    return true;
}
inline bool load_selection(const std::string& path, Selection& sel) {
    FILE* fp = std::fopen(path.c_str(),"rb");
    if (!fp) return false;
    char buff[256]{};
    const size_t n = std::fread(buff,1,sizeof(buff)-1,fp);
    std::fclose(fp);
    if (n == 0 || n >= sizeof(buff)-1) return false;
    const std::string data(buff,n);
    if (data.rfind("BGH_THEME_V1\n",0) != 0) return false;
    const auto a = data.find('\n',13);
    if (a == std::string::npos) return false;
    const auto b = data.find('\n',a+1);
    if (b == std::string::npos || b+1 != data.size()) return false;
    Selection s{data.substr(13,a-13),data.substr(a+1,b-a-1)};
    ThemeItem ignored;
    if (!find(Kind::background,s.background,ignored) ||
        !find(Kind::logo,s.logo,ignored)) return false;
    sel = s;
    return true;
}
inline bool remove_user_image(Kind kind, const std::string& id,
                              const Selection& sel, bool user_confirmed) {
    // Must be explicitly invoked from a confirmation UI. Never delete built-ins.
    if (!user_confirmed || id.rfind("user:",0) != 0) return false;
    if ((kind == Kind::background && id == sel.background) ||
        (kind == Kind::logo && id == sel.logo)) return false;
    const std::string filename = id.substr(5);
    if (!valid_filename(filename)) return false;
    ThemeItem image;
    if (!find(kind,id,image) || !image.user_supplied) return false;
    // Restrict deletion to the three known theme directories (never assets).
    bool approved = false;
    for (const char* folder : {imported_folder(kind),upload_folder(kind),
                                app0_upload_folder(kind),legacy_upload_folder(kind)}) {
        if (image.path == std::string(folder) + "/" + filename) approved = true;
    }
    if (!approved) return false;
    struct stat st{};
    if (::lstat(image.path.c_str(),&st) != 0 || !S_ISREG(st.st_mode)) return false;
    return ::unlink(image.path.c_str()) == 0;
}
inline bool logo_should_render(const Selection& sel) {
    // No opendir/lstat during every rendered frame.
    return sel.logo != "none" && sel.background != "mountain_crown";
}
} // namespace bgh_gallery
} // namespace ulg
