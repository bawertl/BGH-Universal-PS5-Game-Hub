// BGH V1.0.10: frame-time-based, single-threaded cover-load debounce.
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>

namespace ulg {
class CoverLoadDebounce {
public:
    static constexpr float kQuietSeconds = 0.035f;

    void advance(float dt, bool navigation_event, int selected, int tab) {
        // Tracking selection and platform as well as the current frame's input
        // protects programmatic jumps (recent -> game, tabs, search rebuilds).
        const bool changed = !known_ || selected != selected_ || tab != tab_;
        known_ = true;
        selected_ = selected;
        tab_ = tab;
        if(navigation_event || changed){quiet_seconds_ = 0.0f; return;}
        // Ignore negative/broken timestamps; cap long frame gaps to avoid
        // considering a multi-second hitch to be an instantaneous idle period.
        const float safe_dt = std::clamp(dt, 0.0f, 0.10f);
        quiet_seconds_ = std::min(quiet_seconds_ + safe_dt, 1.0f);
    }

    bool allow_regular_covers() const {return quiet_seconds_ >= kQuietSeconds;}
    void reset() {quiet_seconds_ = 0.0f; known_ = false;}

private:
    float quiet_seconds_ = 0.0f;
    int selected_ = 0;
    int tab_ = 0;
    bool known_ = false;
};
} // namespace ulg
