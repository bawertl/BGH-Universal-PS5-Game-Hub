// PS5 Universal Launcher - portable PNG (8-bit) image decoding.
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace ulg {
struct PngImage {int width=0,height=0;std::vector<std::uint8_t> rgba;};
// PNG 8-bit grayscale/RGB/indexed/gray+alpha/RGBA, noninterlaced; zlib required.
// Invalid or too-large pictures fail safely and get a platform-colour fallback.
bool decode_png(const std::string &filename, PngImage &result);
// UI assets can preserve native resolution; zero means no downscale.
bool decode_png(const std::string &filename, PngImage &result,int max_edge);
} // namespace ulg
