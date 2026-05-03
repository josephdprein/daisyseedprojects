// ILed.h — single-channel LED brightness output.
//
// Per SPECIFICATION.md §Core Interfaces. Concrete impls: DaisyLed (hardware
// PWM via daisy::Pwm) for firmware, MockLed (records history) for host tests.
//
// Clamping policy: the interface accepts any float; implementations are
// responsible for clamping to [0, 1] before writing the underlying device.
// This keeps the interface trivial and lets tests inspect the raw value the
// envelope produced.

#pragma once

class ILed {
public:
    virtual ~ILed() = default;

    // Set the LED brightness. Nominal range is [0, 1]; values outside this
    // range MUST be clamped by the implementation, not the caller.
    virtual void SetBrightness(float v01) = 0;
};
