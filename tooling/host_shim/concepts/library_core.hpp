// PS5 Universal Launcher - portable, read-only, POSIX game directory scanner.
// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace ulg {
struct Root { std::string platform, path; bool enabled = true; };
struct Game {
    std::string id, title, platform, path, format, title_id, cover_path;
    bool available = true;
    bool via_ftp = false; // True only when localhost ftpsrv verified its directory listing.
};
struct NativeMetadata { std::string title, title_id; };
// Parse bounded JSON/SFO content without reading or modifying game files.
NativeMetadata native_metadata_from_bytes(const std::string &json, const std::string &sfo);
// Strong evidence first; ambiguous native apps remain Unbekannt.
std::string classify_native_game(const std::string &folder, const NativeMetadata &meta, bool json_present, bool sfo_present);
struct RootState {
    std::string platform, path, status;
    int error_code = 0; // native POSIX errno, 0 if none
    std::string operation; // lstat / opendir / readdir / type
}; // ok / offline / usb_offline / zugriff_verweigert / fehler / deaktiviert
struct Catalogue {
    std::vector<Game> games;
    std::vector<RootState> roots;
    std::vector<std::string> errors;
    bool truncated = false;
    std::size_t failed_operations = 0;
    std::size_t failed_roots = 0;
    std::size_t disabled_roots = 0;
};
// Distinguishes POSIX errors. USB mount state remains a best-effort determination.
RootState make_root_error(const std::string &platform,const std::string &path,
                          const std::string &operation,int error_code);
RootState probe_root(const Root &root);
// With no file, returns conservative documented default candidates; never writes.
std::vector<Root> load_roots(const std::string &config_path);
Catalogue scan(const std::vector<Root> &roots, int max_depth = 5, std::size_t max_games = 5000);
// Host/test utility. Caller chooses output path outside game folders.
// Applies explicit game-ID or absolute game-path -> absolute local PNG cover overrides.
void apply_cover_overrides(Catalogue &catalogue, const std::string &config_path);
// Exact title ID, stable game ID, or absolute path => confirmed display title.
void apply_title_overrides(Catalogue &catalogue, const std::string &config_path);
std::string clean_title_for_display(const std::string &title);
Catalogue merge_unavailable(Catalogue fresh, const Catalogue &previous);
// Library-only policy: hide non-emulator apps, drop disabled-root entries and
// coalesce known title IDs across several installation paths (never deletes files).
void clean_catalogue(Catalogue &catalogue, const std::vector<Root> &configured_roots);
bool is_emulator_title_id(const std::string &title_id);
// Host/test utility. Caller chooses output path outside game folders.
bool write_json(const Catalogue &catalogue, const std::string &output_path);
} // namespace ulg
