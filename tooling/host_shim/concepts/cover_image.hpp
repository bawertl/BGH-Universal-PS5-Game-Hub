// BGH V0.8: PNG plus bounded JPEG/TGA decoder for local cover art.
// SPDX-License-Identifier: MIT
#pragma once
#include "cover_png.hpp"
#include <string>
namespace ulg {
// Returns false (never throws) for invalid, unsupported, oversized images.
bool decode_cover_image(const std::string &file,PngImage &result,std::string *failure=nullptr);
bool decode_ui_image(const std::string &file,PngImage &result,std::string *failure=nullptr);
}
