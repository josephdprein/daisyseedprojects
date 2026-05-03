// LedTrigger.h — one-shot LED envelope + PulseConfirm pattern utility.
//
// Per SPECIFICATION.md §DrumPad Behavior and tasks/03-led-trigger.md:
//   - Fire(nowMs) schedules a linear ramp:
//       0 → 1 over [nowMs, nowMs + kLedAttackMs)
//       1 → 0 over [nowMs + kLedAttackMs, nowMs + kLedAttackMs + kLedDecayMs)
//       0 thereafter.
//   - PulseConfirm(nowMs) plays the pattern (on / gap / on / off), repeated
//     kPulseConfirmCount times. "on" segments hold 1.0; gaps and trailing
//     hold 0.0.
//   - PulseConfirm wins: calling PulseConfirm while a Fire envelope is active
//     cancels the Fire — only the pulse pattern drives the output.
//   - Brightness(nowMs) is a pure function of internal state at nowMs; the
//     caller advances time. No implicit time-stepping.
//
// No heap allocation: the class stores a single discriminated mode plus the
// start timestamp. All segment math is done on the fly using Config constants.
//
// Time-source contract: nowMs is monotonically non-decreasing. Brightness()
// passed an nowMs older than the last Fire/PulseConfirm start is undefined
// (callers in this firmware always feed daisy::System::GetNow()).

#pragma once

#include <cstdint>

namespace drum_machine {

class LedTrigger {
public:
    LedTrigger() = default;

    // Schedule a Fire envelope starting at nowMs. Replaces any prior state.
    void Fire(uint32_t nowMs);

    // Start a PulseConfirm pattern at nowMs. Cancels any active Fire envelope
    // (PulseConfirm wins, per spec).
    void PulseConfirm(uint32_t nowMs);

    // Pure query: brightness in [0, 1] for the supplied wall-clock nowMs.
    float Brightness(uint32_t nowMs) const;

    // Reset to idle (brightness 0). Used by DrumMachine::Init() for the
    // panic-reset path.
    void Reset();

private:
    enum class Mode : uint8_t {
        Idle    = 0,
        Fire    = 1,
        Pulse   = 2,
    };

    Mode     mode_  = Mode::Idle;
    uint32_t start_ = 0;
};

}  // namespace drum_machine
