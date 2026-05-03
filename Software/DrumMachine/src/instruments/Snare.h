// Snare.h — IInstrument wrapping daisysp::AnalogSnareDrum.
//
// Per SPECIFICATION.md §Instruments & Randomization:
//   Randomized params: freq, decay, snappy, tone.
//   Fixed param:       accent.
//   Decay normalized 0..1 (DaisySP convention).

#pragma once

#include <array>
#include <cstddef>

#include "Drums/analogsnaredrum.h"

#include "IInstrument.h"
#include "../randomization/IRng.h"
#include "../randomization/RandomizationProfile.h"

namespace drum_machine {

class Snare final : public IInstrument {
public:
    // Order: 0 freq, 1 decay, 2 snappy, 3 tone.
    static constexpr std::size_t kNumRandomized = 4;

    Snare();

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
    daisysp::AnalogSnareDrum               voice_;
    RandomizationProfile<kNumRandomized>   profile_;
    std::array<float, kNumRandomized>      current_{};
    float                                  depth_   = 0.0f;
    float                                  accent_  = 0.7f;
};

}  // namespace drum_machine
