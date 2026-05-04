// Resonator.h — IInstrument wrapping daisysp::ModalVoice.
//
// Per SPECIFICATION.md §Instruments & Randomization:
//   Randomized params: freq, structure, brightness, damping.
//   Fixed param:       accent.

#pragma once

#include <array>
#include <cstddef>

#include "PhysicalModeling/modalvoice.h"

#include "IInstrument.h"
#include "../randomization/IRng.h"
#include "../randomization/RandomizationProfile.h"

namespace drum_machine {

class Resonator final : public IInstrument {
public:
    // Order: 0 freq, 1 structure, 2 brightness, 3 damping.
    static constexpr std::size_t kNumRandomized = 4;

    Resonator();

    void  Init(float sampleRate, float depth01) override;
    void  Trig() override;
    float Process() override;
    void  Randomize(IRng& rng) override;
    void  RestorePreviousTrig() override;

#if defined(DRUMMACHINE_HOST_TEST)
    const std::array<float, kNumRandomized>& Snapshot() const { return current_; }
    float AccentSnapshot() const { return accent_; }
    const RandomizationProfile<kNumRandomized>& Profile() const { return profile_; }
#endif

private:
    daisysp::ModalVoice                    voice_;
    RandomizationProfile<kNumRandomized>   profile_;
    std::array<float, kNumRandomized>      current_{};
    // Two-deep Trig() history; see BassDrum.h for the contract.
    std::array<float, kNumRandomized>      prev_{};
    std::array<float, kNumRandomized>      prev_prev_{};
    float                                  depth_   = 0.0f;
    float                                  accent_  = 0.7f;
};

}  // namespace drum_machine
