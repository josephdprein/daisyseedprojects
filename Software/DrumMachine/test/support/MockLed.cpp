// MockLed.cpp — implementation of the recording test LED.

#include "MockLed.h"

void MockLed::SetBrightness(float v01) {
    // Clamp to [0, 1] per the ILed contract (callers may legitimately hand
    // us out-of-range values; the implementation owns the clamp).
    float clamped = v01;
    if (clamped < 0.0f) {
        clamped = 0.0f;
    } else if (clamped > 1.0f) {
        clamped = 1.0f;
    }
    history_.emplace_back(nowMs_, clamped);
}

void MockLed::SetNow(uint32_t nowMs) { nowMs_ = nowMs; }

const std::vector<std::pair<uint32_t, float>>& MockLed::BrightnessHistory()
    const {
    return history_;
}

float MockLed::Last() const {
    if (history_.empty()) {
        return 0.0f;
    }
    return history_.back().second;
}
