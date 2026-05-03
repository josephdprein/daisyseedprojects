// DaisyLed.h — ILed backed by a hardware-PWM channel on a daisy::PWMHandle.
//
// Per SPECIFICATION.md §Hardware Contract: LED brightness MUST be driven by
// hardware PWM (software PWM is explicitly out of scope). The chosen libDaisy
// API is `daisy::PWMHandle` (per/pwm.h), with brightness applied via
// `PWMHandle::Channel::Set(float v)` — that helper clamps to [0, 1] and
// normalizes to the timer's period internally.
//
// Pin / timer constraint: the four LEDs share one TIM4 PWMHandle. TIM4
// channels 1..4 map to pins PB6/PB7/PB8/PB9 = Daisy Seed D13/D14/D11/D12
// respectively (see per/pwm.h's table). If the hardware integrator changes
// the LED pins they must remain on a single timer and a known channel index.
//
// Ownership: DaisyLed does NOT own the PWMHandle. main.cpp owns the shared
// PWMHandle and initializes it once (one peripheral, four channels). Each
// DaisyLed binds to a specific channel index (1..4) at Init() time.

#pragma once

#include "daisy_seed.h"
#include "per/pwm.h"

#include "ILed.h"

namespace drum_machine {

class DaisyLed final : public ILed {
public:
    DaisyLed() = default;

    // Bind this LED to the given channel of an already-Init()'d PWMHandle.
    // `channelIndex` is 1..4 (matching TIM4 CCR1..CCR4 / PWMHandle::Channel1..4).
    // The shared `pwm` reference must outlive this object.
    void Init(daisy::PWMHandle& pwm, int channelIndex);

    // ILed — clamps to [0, 1] internally (delegated to PWMHandle::Channel::Set,
    // which performs the same clamp before scaling to the configured period).
    void SetBrightness(float v01) override;

private:
    daisy::PWMHandle::Channel* channel_ = nullptr;
};

}  // namespace drum_machine
