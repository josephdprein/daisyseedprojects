// DaisyButton.h — IButton backed by daisy::Switch.
//
// Per SPECIFICATION.md §Hardware Contract and tasks/09-firmware-integration.md:
//   - Hardware: 4× Adafruit arcade button, active-low with internal pull-up.
//   - Init wires Switch::TYPE_MOMENTARY + POLARITY_INVERTED + PULL_UP.
//   - Update(nowMs) calls Switch::Debounce() once and then snapshots
//     Pressed() / RisingEdge() / FallingEdge() / TimeHeldMs() so the IButton
//     edge accessors describe the most recent Update() only — matching
//     MockButton's contract. (Switch's own RisingEdge/FallingEdge are also
//     edge-of-last-Debounce, but we still snapshot to make the contract
//     obvious at the call site and to keep IsDown/HeldMs cheap.)
//
// Lifetime: this class holds a daisy::Switch by value. Init() must be called
// once (from main.cpp) with the audio sample rate before any Update() call.

#pragma once

#include <cstdint>

#include "daisy_seed.h"
#include "hid/switch.h"

#include "IButton.h"

namespace drum_machine {

class DaisyButton final : public IButton {
public:
    DaisyButton() = default;

    // Initialize the underlying daisy::Switch. `pin` typically comes from
    // Config::kButtonPins[i]. `sampleRate` is forwarded for backwards
    // compatibility (current libDaisy ignores it inside Switch).
    void Init(daisy::Pin pin, float sampleRate);

    // IButton — see IButton.h for contract.
    void     Update(uint32_t nowMs) override;
    bool     IsDown() const override        { return is_down_; }
    bool     JustPressed() const override   { return just_pressed_; }
    bool     JustReleased() const override  { return just_released_; }
    uint32_t HeldMs() const override        { return held_ms_; }

private:
    daisy::Switch sw_{};
    bool          is_down_       = false;
    bool          just_pressed_  = false;
    bool          just_released_ = false;
    uint32_t      held_ms_       = 0;
};

}  // namespace drum_machine
