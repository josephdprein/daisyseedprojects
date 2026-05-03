// BassDrum.h — IInstrument wrapping daisysp::AnalogBassDrum.
//
// Per SPECIFICATION.md §Instruments & Randomization:
//   Randomized params: freq, decay, tone (mapped to SetSelfFmAmount).
//   Fixed param:       accent (set once at Init() to baseline; never re-rolled).
//   Decay is normalized 0..1 (DaisySP convention), NOT seconds.
//
// Owns the DaisySP voice by value (no heap). Owns a RandomizationProfile<3>
// describing the [min, max, baseline] of each randomized parameter; ranges
// here are the implementer's first pass per the spec's range tuning policy.

#pragma once

#include <array>
#include <cstddef>

#include "Drums/analogbassdrum.h"

#include "IInstrument.h"
#include "../randomization/IRng.h"
#include "../randomization/RandomizationProfile.h"

namespace drum_machine {

class BassDrum final : public IInstrument {
public:
    // Ordering of randomized params in Snapshot()/profile_:
    //   0: freq, 1: decay, 2: tone (SelfFmAmount).
    static constexpr std::size_t kNumRandomized = 3;

    BassDrum();

    void  Init(float sampleRate, float depth01) override;
    void  Trig() override;
    float Process() override;
    void  Randomize(IRng& rng) override;

#if defined(DRUMMACHINE_HOST_TEST)
    // Test-only: returns the most-recently-applied randomized parameter values.
    // Order matches profile_ entries above.
    const std::array<float, kNumRandomized>& Snapshot() const { return current_; }

    // Test-only: returns the (fixed) accent value last applied.
    float AccentSnapshot() const { return accent_; }

    // Test-only access to the profile so randomization tests can compute
    // expected values without re-declaring the ranges.
    const RandomizationProfile<kNumRandomized>& Profile() const { return profile_; }
#endif

private:
    daisysp::AnalogBassDrum                voice_;
    RandomizationProfile<kNumRandomized>   profile_;
    std::array<float, kNumRandomized>      current_{};
    float                                  depth_   = 0.0f;
    float                                  accent_  = 0.7f;
};

}  // namespace drum_machine
