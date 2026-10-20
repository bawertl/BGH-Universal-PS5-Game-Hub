// PS5 Universal Game Launcher - read-only folder browser and private root settings.
// SPDX-License-Identifier: MIT
#pragma once
#include "library_core.hpp"
#include <string>
#include <vector>
namespace ulg {
bool valid_root(const Root &root);
// Adds two independent paths per missing PS4/PS5 platform; never deletes existing roots.
bool ensure_native_roots(std::vector<Root>& roots);
// Transient root for legacy user configs already scanning /mnt/usb1 subfolders.
// Never saves or re-enables an explicitly disabled PS5 root.
bool ensure_ps5_usb_parent_root(std::vector<Root>& roots);
// Append the verified internal PS4 PKG installation root to older saved configurations.
// Existing entries (including disabled ones) remain untouched.
bool ensure_ps4_install_root(std::vector<Root>& roots);
// Writes only the caller-selected preferences file, never under scanned roots.
bool save_user_roots(const std::vector<Root> &roots, const std::string &private_path);
// If no valid personal file exists, use the read-only packaged defaults.
std::vector<Root> configured_roots(const std::string &private_path,
                                   const std::string &packaged_path);
struct BrowserEntry {std::string name, path;};
struct BrowserResult {
    std::string path;
    std::vector<BrowserEntry> entries;
    RootState state;
};
// Each entry is a real, direct directory (no symbolic link traversal).
BrowserResult browse_folders(const std::string &path, std::size_t max_entries=120);
} // namespace ulg
