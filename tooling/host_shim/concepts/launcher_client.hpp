// Protocol: heydemoura/ps5-forwarder-format SPEC.md section 7 (GPL project; independent client).
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace ulg {
enum class LaunchStatus { started, queued, no_launcher, refused, invalid_target, invalid_path, protocol_error };
struct LaunchPlan {
    std::string target;
    std::vector<std::string> argv;
    std::string reason;
    bool supported = false;
};
struct LaunchResponse {
    LaunchStatus status = LaunchStatus::protocol_error;
    std::uint32_t stage = 0;
    std::uint32_t rc1 = 0;
    std::uint32_t rc2 = 0;
    std::string message;
};
// TCP connection probes only. They send NO payload bytes and no launch request.
// An open port proves reachability, NOT the identity or correctness of its service.
// A connection can fail at different stages. Preserve the stage instead of
// assigning every error to a non-existent listener or an app permission issue.
enum class PortProbeStage {
    none, invalid_arguments, socket_create, nonblocking, connect_call,
    select_wait, socket_status
};
struct LocalPortProbe {
    bool reachable = false;
    int os_error = 0; // errno of the failed call, or SO_ERROR after select
    PortProbeStage stage = PortProbeStage::none;
};
struct LocalLaunchServices {
    LocalPortProbe launcher; // 127.0.0.1:10199
    LocalPortProbe elfldr;   // 127.0.0.1:9021
    LocalPortProbe ftp;      // 127.0.0.1:2121 (BGH's existing FTP fallback port)
};
const char *probe_stage_name(PortProbeStage stage);
const char *probe_errno_name(int error);
LocalPortProbe probe_loopback_tcp(unsigned short port, int timeout_ms = 300);
LocalLaunchServices probe_local_launch_services();

// Only documented targets. PS5SX2/PS5/PS4 intentionally excluded.
LaunchPlan make_launch_plan(const std::string &platform, const std::string &game_path);
// Native PS5 titles: request registered PPSA game through an ALREADY RUNNING
// PS5 Forwarder Format launcher. No service installation or auto-injection.
// Deliberately excludes BGH and known emulator/homebrew title IDs.
LaunchPlan make_registered_ps5_launch_plan(const std::string &title_id);
LaunchPlan make_registered_ps4_launch_plan(const std::string &title_id);
LaunchPlan make_ps5sx2_menu_launch_plan();
LaunchPlan make_prosperoeden_menu_launch_plan();
LaunchPlan make_ps5x360_menu_launch_plan();
// Caller initiated only, never automatically invoked by scanning or app startup.
// Needs an already running loopback launcher; NEVER injects ELF or changes system state.
LaunchResponse send_launch(const LaunchPlan &plan, unsigned short port = 10199);
} // namespace ulg
