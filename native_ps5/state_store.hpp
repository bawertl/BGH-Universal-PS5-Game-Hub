// PS5 Universal Launcher - bounded, checksum-protected local state.
// SPDX-License-Identifier: MIT
#pragma once
#include "library_core.hpp"
#include <cstdint>
#include <set>
#include <string>
#include <vector>
namespace ulg {
struct Preferences { std::set<std::string> favorites; std::vector<std::string> recent; };
// A separate, versioned and checksummed file preserves the existing ULGP
// preferences.bin format, favorites, and recent launch history byte for byte.
struct EmulatorBootSettings {
    bool direct_boot=true;
    std::uint32_t revision=0;
};
// Independent language choice; never rewrite ULGP favorites or ULGE boot mode.
struct LanguageSettings {
    bool english=false;
    std::uint32_t revision=0;
};
// Separately stored optical theme; no changes to saved games, favorites or boot mode.
struct ThemeSettings {
    bool frosted_glass=true;
    std::uint32_t revision=0;
};
// Independent BGM switch. Keeps ULGP/ULGE/ULGL/ULGT files byte-compatible.
struct MusicSettings {
    bool enabled=true;
    std::uint32_t revision=0;
};
// Diagnostics are observational. Existing callers may omit the optional pointer.
// stage identifies the failing operation; os_error is captured immediately at failure.
struct StorageStatus {
    bool ok=false;
    std::string stage="not_run";
    int os_error=0;
};
// Only write these files in the launcher's OWN writable title storage, never inside a game tree.
bool write_catalogue_cache(const Catalogue &catalogue, const std::string &path);
bool read_catalogue_cache(const std::string &path, Catalogue &out);
bool write_preferences(const Preferences &preferences, const std::string &path, StorageStatus *status=nullptr);
bool read_preferences(const std::string &path, Preferences &out, StorageStatus *status=nullptr);
bool write_emulator_boot_settings(const EmulatorBootSettings &settings,const std::string &path,StorageStatus *status=nullptr);
bool read_emulator_boot_settings(const std::string &path,EmulatorBootSettings &out,StorageStatus *status=nullptr);
bool write_language_settings(const LanguageSettings &settings,const std::string &path,StorageStatus *status=nullptr);
bool read_language_settings(const std::string &path,LanguageSettings &out,StorageStatus *status=nullptr);
bool write_theme_settings(const ThemeSettings &settings,const std::string &path,StorageStatus *status=nullptr);
bool read_theme_settings(const std::string &path,ThemeSettings &out,StorageStatus *status=nullptr);
bool write_music_settings(const MusicSettings &settings,const std::string &path,StorageStatus *status=nullptr);
bool read_music_settings(const std::string &path,MusicSettings &out,StorageStatus *status=nullptr);
} // namespace ulg
