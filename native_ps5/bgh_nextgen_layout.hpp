// SPDX-License-Identifier: MIT
#pragma once
#include <array>
namespace ulg::bgh_layout {
constexpr float canvas_width=1920.0f;
constexpr float center_x=canvas_width*0.5f; // 960 logical px (also when output is 4K)
constexpr std::array<float,7> card_offset{{-730.f,-510.f,-284.f,0.f,284.f,510.f,730.f}};
constexpr std::array<float,7> card_width{{134.f,160.f,185.f,300.f,185.f,160.f,134.f}};
constexpr std::array<float,7> card_alpha{{0.68f,0.70f,0.73f,1.0f,0.73f,0.70f,0.68f}};
constexpr float card_center(unsigned index) { return center_x+card_offset[index]; }
constexpr float card_left(unsigned index) { return card_center(index)-card_width[index]*0.5f; }
inline float centered_x(float element_width) {return center_x-element_width*0.5f;}
inline float text_x(float text_width) {return card_left(3)+card_width[3]*0.5f-text_width*0.5f;}
} // namespace ulg::bgh_layout
