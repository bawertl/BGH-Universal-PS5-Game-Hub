// BGH V0.8: read-only emulator artwork lookup; only writes to the BGH private cache.
// SPDX-License-Identifier: MIT
#pragma once
#include "library_core.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace ulg {
// ProsperoEden headless/storage_paths.h: FNV-1a 32-bit of ROM file NAME, not full path.
std::uint32_t eden_cover_hash(const std::string &filename);
std::string eden_cover_filename(const std::string &rom_path);
// PS5X360 src/covers.cpp: XEX2 execution info optional header 0x00040006, title ID at +12.
std::string xex_title_id(const std::string &header);
// Conservative supported image signatures: rejects other files before copying them.
bool cover_signature(const std::string &bytes,const std::string &extension);
// Cover sources are fixed platform defaults on PS5; test fixtures may override them.
struct EmulatorCoverSources {
    std::string ps2_manual="/data/PCSX2/covers";
    std::string ps2_cache="/data/PCSX2/cache/covers";
    std::string ps2_serial_cache="/data/PCSX2/cache/chd-serials.txt";
    std::string switch_covers="/data/prosperoeden/covers";
    std::string xbox_covers="/download0/xbox360ps5/covers";
    std::string xbox_installed_covers="/data/homebrew/PPSA50011/assets/covers";
};
// Existing emulator artwork only. No network cover-service downloads.
// Results persist in BGH's private directory, original files remain untouched.
class EmulatorCoverImporter {
public:
    explicit EmulatorCoverImporter(std::string cache_dir, int ftp_port=2121,
                                   EmulatorCoverSources sources={});
    // One game per call, preferably in visible order. No effect on the game metadata.
    bool hydrate(Game &game,std::string *diagnostic=nullptr);
private:
    std::string cache_dir_;
    int ftp_port_;
    EmulatorCoverSources sources_;
};
}
