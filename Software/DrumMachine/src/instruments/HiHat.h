// HiHat.h — IInstrument wrapping daisysp::HiHat<>.
//
// Template defaults chosen: <SquareNoise, LinearVCA, true> — DaisySP defaults
// for an 808-flavoured hi-hat. SquareNoise + LinearVCA + resonance=true is
// the canonical combination for a recognizable closed/open hat tone, and is
// the option pre-verified by the orchestrator note.
//
// Per SPECIFICATION.md §Instruments & Randomization:
//   Randomized params: freq, decay, noisiness, tone.
//   Fixed param:       accent.

#pragma once

#include <array>
#include <cstddef>

#include "Drums/hihat.h"

#include "IInstrument.h"
#include "../randomization/IRng.h"
#include "../randomization/RandomizationProfile.h"

namespace drum_machine {

class HiHat final : public IInstrument {
public:
    // Order: 0 freq, 1 decay, 2 noisiness, 3 tone.
    static constexpr std::size_t kNumRandomized = 4;

    HiHat();

    void  Init(float sampleRate, float depth01) override;
    void  Trig() override;
    float Process() override;
    void  Randomize(IRng& rng) override;

#if defined(DRUMMACHINE_HOST_TEST)
    const std::array<float, kNumRandomized>& Snapshot() const { return current_; }
    float AccentSnapshot() const { return accent_; }
    const RandomizationProfile<kNumRandomized>& Profile() const { return profile_; }
#endif

private:
    // Default template params (SquareNoise, LinearVCA, resonance=true) — see
    // hihat.h. The header gates `resonance` into the SVF setup at compile time.
    daisysp::HiHat<>                       voice_;
    RandomizationProfile<kNumRandomized>   profile_;
    std::array<float, kNumRandomized>      current_{};
    float                                  depth_   = 0.0f;
    float                                  accent_  = 0.7f;
};

}  // namespace drum_machine
