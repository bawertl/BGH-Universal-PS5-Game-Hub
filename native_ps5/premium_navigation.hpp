// BGH V0.12: input and camera math shared by the native GUI and host tests.
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace ulg::premium {
inline int step(int selected, int delta, std::size_t count) {
    if(count==0)return 0;
    return std::clamp(selected+delta,0,static_cast<int>(count)-1);
}
inline float scroll_target(int selected, std::size_t count, int visible) {
    if(count<=static_cast<std::size_t>(visible))return 0.0f;
    const int tail=static_cast<int>(count)-visible;
    return static_cast<float>(std::clamp(selected-visible/2,0,tail));
}
inline float approach(float current,float target,float dt,float speed) {
    if(!std::isfinite(current)||!std::isfinite(target))return target;
    if(std::abs(current-target)>8.0f)return target; // Fast key repeats: no queued animation.
    const float blend=1.0f-std::exp(-speed*std::clamp(dt,0.0f,0.10f));
    if(std::abs(current-target)<0.003f)return target;
    return current+(target-current)*blend;
}
}
