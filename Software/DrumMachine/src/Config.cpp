// Config.cpp — pin map definitions for the DrumMachine firmware.
//
// PLACEHOLDER PIN MAP — replace before flashing real hardware.
// LED pins MUST be on a timer-PWM-capable channel (see SPECIFICATION.md
// §Hardware Contract). Buttons are active-low with internal pull-up.
//
// Current LED pin choice maps to TIM4's four channels (per
// libDaisy per/pwm.h):
//   - kLedPins[0] = D13 → TIM4 channel 1 (PB6, Bass)
//   - kLedPins[1] = D14 → TIM4 channel 2 (PB7, Snare)
//   - kLedPins[2] = D11 → TIM4 channel 3 (PB8, HiHat)
//   - kLedPins[3] = D12 → TIM4 channel 4 (PB9, Resonator)
// The channel index used for each PadIndex is encoded in main.cpp; if you
// reshuffle these LED pins you must keep them on a single TIM peripheral
// and update main.cpp to bind the matching channel for each pad.
//
// Buttons: D7..D10 are plain GPIOs — no timer requirement, just need
// internal-pull-up support (every Daisy Seed digital pin does). They map
// straight to PadIndex order.

#include "Config.h"

namespace Config {

const daisy::Pin kButtonPins[static_cast<size_t>(PadIndex::Count)] = {
    daisy::seed::D7,   // Bass
    daisy::seed::D8,   // Snare
    daisy::seed::D9,   // HiHat
    daisy::seed::D10,  // Resonator
};

const daisy::Pin kLedPins[static_cast<size_t>(PadIndex::Count)] = {
    daisy::seed::D13,  // Bass      → TIM4 CH1 (PB6)
    daisy::seed::D14,  // Snare     → TIM4 CH2 (PB7)
    daisy::seed::D11,  // HiHat     → TIM4 CH3 (PB8)
    daisy::seed::D12,  // Resonator → TIM4 CH4 (PB9)
};

}  // namespace Config
