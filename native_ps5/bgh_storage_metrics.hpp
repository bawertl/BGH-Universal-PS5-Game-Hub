// SPDX-License-Identifier: MIT
#pragma once
// BGH V1.1.11: PS5 startup-safe placeholder. No statvfs or filesystem probes.
// Do not infer device capacity until a verified PS5 API is available.
#include <cstdint>
#include <string>
namespace ulg::bgh_storage {
struct Volume {
    std::string mount;
    std::uint64_t capacity_bytes=0,used_bytes=0,available_bytes=0;
    bool known=false;
    int error=0;
    double percent() const { return 0.0; }
};
inline Volume probe(const char*) { return {}; }
inline Volume internal() { return {}; }
inline Volume external() { return {}; }
inline std::string label(const Volume&) { return "Nicht verfuegbar"; }
} // namespace ulg::bgh_storage
