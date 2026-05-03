// Mixer.h — sums four IInstrument outputs into a mono signal with per-voice
// gains.
//
// Per SPECIFICATION.md §Mixer:
//   peak(buffer) = max |sample|. With all four pads triggered at full accent,
//   the captured buffer must satisfy peak ≤ 0.95 (no hard clipping). Per-voice
//   peaks ≥ 0.05 in isolation are asserted in test_instruments.cpp. The two
//   bounds together drive gain tuning.
//
// Ownership: Mixer borrows the four IInstrument pointers; the caller (main.cpp
// or TestRig in tests) owns the underlying voices. No heap allocation after
// construction.

#pragma once

#include <array>
#include <cstddef>

#include "instruments/IInstrument.h"
#include "PadIndex.h"

namespace drum_machine {

class Mixer {
public:
    static constexpr std::size_t kNumVoices =
        static_cast<std::size_t>(::PadIndex::Count);

    // Constructor takes the four instrument pointers in PadIndex order
    // (Bass, Snare, HiHat, Resonator). Pointers must remain valid for the
    // lifetime of the Mixer.
    explicit Mixer(std::array<IInstrument*, kNumVoices> instruments);

    // Compute one mono sample as the gain-weighted sum of the four
    // instrument.Process() outputs. Audio-rate; no allocation, no logging.
    float Process();

    // Set the per-voice gain. Index uses PadIndex casting (0..3). Allowed
    // pre-init or anytime; tests use it to probe individual voices through the
    // mixer summing path.
    void  SetGain(std::size_t voice, float gain);
    float GetGain(std::size_t voice) const;

private:
    std::array<IInstrument*, kNumVoices> instruments_;
    std::array<float,        kNumVoices> gain_;
};

}  // namespace drum_machine
