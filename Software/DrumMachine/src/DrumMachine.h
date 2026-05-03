// DrumMachine.h — top-level engine: owns four DrumPads + one Mixer.
//
// Per SPECIFICATION.md §DrumMachine API and tasks/08-drummachine-and-testrig.md:
//   - Constructor borrows four IButton*, four ILed*, four IInstrument*, and a
//     single IRng& (one shared RNG drives every pad's randomization). All
//     references must outlive this DrumMachine.
//   - Init(sampleRate) is idempotent; calling it a second time performs a full
//     state reset (panic). It forwards (sampleRate, kDefaultRandomizationDepth)
//     to each instrument and re-applies the boot-stuck mask on every pad.
//   - Tick(nowMs) calls each pad's Tick() in PadIndex order so pads pressed in
//     the same control tick all fire that tick.
//   - Process() returns one mono sample from the internal Mixer.
//
// There is no public per-pad accessor — tests interact with the engine via
// TestRig, which queries the same MockLed / MockRng references it injected.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "controls/IButton.h"
#include "controls/ILed.h"
#include "DrumPad.h"
#include "instruments/IInstrument.h"
#include "Mixer.h"
#include "PadIndex.h"
#include "randomization/IRng.h"

namespace drum_machine {

class DrumMachine {
public:
    static constexpr std::size_t kNumPads =
        static_cast<std::size_t>(::PadIndex::Count);

    DrumMachine(
        std::array<IButton*,     kNumPads> buttons,
        std::array<ILed*,        kNumPads> leds,
        std::array<IInstrument*, kNumPads> instruments,
        IRng&                              rng);

    // Idempotent. See header comment.
    void  Init(float sampleRate);

    // Call once per audio block (≤ 2 ms).
    void  Tick(uint32_t nowMs);

    // Call once per audio sample. Returns a mono sample in approximately
    // [-1, 1] (test_mixer asserts peak ≤ 0.95 with all four pads firing).
    float Process();

private:
    std::array<IInstrument*, kNumPads> instruments_;
    // DrumPads are stored by value so we don't allocate. They borrow the
    // button/led/instrument/rng references handed in via the constructor.
    std::array<DrumPad,      kNumPads> pads_;
    Mixer                              mixer_;
};

}  // namespace drum_machine
