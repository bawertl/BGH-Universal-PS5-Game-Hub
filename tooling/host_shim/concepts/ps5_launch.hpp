// SPDX-License-Identifier: MIT
#pragma once
#include <string>
namespace ulg {
struct NativePs5LaunchResult {bool accepted=false;int result_code=0;std::string message;};
bool valid_native_ps5_title_id(const std::string& platform,const std::string& title_id);
// User gesture only. Launches an already registered app by TITLE ID, never by file path.
NativePs5LaunchResult launch_registered_ps5(const std::string& title_id);
// Explicit button only; opens the emulator menu without ROM/config writes.
NativePs5LaunchResult launch_emulator_menu(const std::string& platform);
} // namespace ulg
