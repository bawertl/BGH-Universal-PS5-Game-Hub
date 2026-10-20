// BGH PS5SX2 one-shot request: narrowly scoped loopback FTP write access.
// The existing artwork/scanner FTP reader stays read-only.
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
namespace ulg {
struct Ps2FtpWriteStatus {
    bool ok=false;
    int ftp_reply=0;
    std::string message;
};
// Only the PS5SX2 request; never writes another remote path or a game file.
Ps2FtpWriteStatus ftp_stage_ps2_request(const std::string& payload, int port=2121);
// On FORWARDER rejection only: remove this request if its current bytes still match.
bool ftp_cancel_ps2_request_if_identical(const std::string& payload, int port=2121);
} // namespace ulg
