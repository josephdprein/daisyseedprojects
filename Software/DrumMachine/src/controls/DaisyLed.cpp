// DaisyLed.cpp — see DaisyLed.h.

#include "DaisyLed.h"

namespace drum_machine {

void DaisyLed::Init(daisy::PWMHandle& pwm, int channelIndex) {
    // Resolve the channel reference once. Each TIM4 channel binds to a fixed
    // pin (see per/pwm.h table); we don't own that mapping here — main.cpp
    // does, by initializing each channel with the correct daisy::Pin.
    switch (channelIndex) {
        default:
        case 1: channel_ = &pwm.Channel1(); break;
        case 2: channel_ = &pwm.Channel2(); break;
        case 3: channel_ = &pwm.Channel3(); break;
        case 4: channel_ = &pwm.Channel4(); break;
    }
}

void DaisyLed::SetBrightness(float v01) {
    if (channel_ == nullptr) {
        return;
    }
    // PWMHandle::Channel::Set takes [0, 1], clamps internally, and writes
    // duty = floor(v * scale_) via __HAL_TIM_SET_COMPARE — a single MMIO
    // store, non-blocking, audio-safe.
    channel_->Set(v01);
}

}  // namespace drum_machine
