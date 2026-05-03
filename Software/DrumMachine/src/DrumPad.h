// DrumPad.h — single-pad state machine: ties one IButton + ILed +
// IInstrument + IRng together via an internal LedTrigger envelope.
//
// Per SPECIFICATION.md §DrumPad Behavior and tasks/05-drumpad.md:
//   - On JustPressed: instrument.Trig(), ledTrigger.Fire(nowMs), then (if
//     randomization is enabled) instrument.Randomize(rng). Hold-toggle armed.
//   - While held past Config::kHoldThresholdMs (and hold-toggle still armed):
//     flip randomizationEnabled, ledTrigger.PulseConfirm(nowMs), disarm hold.
//   - On JustReleased: disarm hold-toggle.
//   - Boot-stuck-button mask: if a button reads "down" on the very first Tick
//     after Init(), suppress its press semantics until the button is observed
//     released at least once. Without this, holding a button at power-on would
//     fire all four drums + a randomization. See cpp file for the comment at
//     the masking site.
//
// Init() is idempotent and may be used as a panic-reset: clears LedTrigger
// state, re-arms hold detection, and re-applies the boot-stuck mask flag so
// that the next Tick re-evaluates the button-down-at-init condition.
//
// Ownership: borrows IButton, ILed, IInstrument, IRng by reference. The
// caller owns lifetimes; DrumPad performs no heap allocation.

#pragma once

#include <cstdint>

#include "controls/IButton.h"
#include "controls/ILed.h"
#include "instruments/IInstrument.h"
#include "randomization/IRng.h"
#include "util/LedTrigger.h"

namespace drum_machine {

class DrumPad {
public:
    DrumPad(IButton& button, ILed& led, IInstrument& instrument, IRng& rng);

    // Resets per-pad runtime state. Idempotent.
    //   - Clears the LedTrigger envelope (LED goes dark).
    //   - Re-arms hold detection (holdToggleArmed = false; armed on next press).
    //   - Re-applies the boot-stuck mask: the next Tick will check IsDown()
    //     and, if true, mask press semantics until the button is released.
    // Note: Init does NOT touch randomizationEnabled — it stays at whatever
    // the caller has toggled it to (the spec only mandates state reset for
    // LED/hold/mask, and a panic-reset that re-enables randomization across
    // all pads can be done by the DrumMachine layer if desired).
    void Init();

    // Run one control tick. Call once per audio block at <= 2 ms.
    void Tick(uint32_t nowMs);

    // Test/inspection accessor. The DrumMachine engine has no public per-pad
    // accessor (per spec); this method exists for the TestRig path in task 08
    // and for direct unit tests of DrumPad in task 05.
    bool RandomizationEnabled() const { return randomizationEnabled_; }

private:
    IButton&     button_;
    ILed&        led_;
    IInstrument& instrument_;
    IRng&        rng_;
    LedTrigger   ledTrigger_;

    bool randomizationEnabled_   = true;
    bool holdToggleArmed_        = false;
    bool firstTickAfterInit_     = true;
    bool pressMaskedUntilRelease_ = false;
};

}  // namespace drum_machine
