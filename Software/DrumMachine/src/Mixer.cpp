// Mixer.cpp — see Mixer.h.
//
// Per-voice gain values (TUNED — final values).
//   Bass      x 6.0    DaisySP AnalogBassDrum is markedly quieter than the
//                       other three voices; the existing GuitarPedal drum
//                       module compensates with the same factor (see
//                       drum_module.cpp:364). Per-voice peak ≈ 0.06 raw, so
//                       at gain 6.0 the bass contributes ≈ 0.35 peak.
//   Snare     x 0.25   AnalogSnareDrum at depth=0.5 routinely peaks above 1.0
//                       on its own (~1.37 worst case across the corpus); 0.25
//                       brings its contribution into ≈ 0.34 peak so the
//                       all-four-firing sum stays under 0.95.
//   HiHat     x 0.45   HiHat<> peaks ≈ 0.31 raw; 0.45 keeps it audible
//                       (~0.14 contribution) without clipping.
//   Resonator x 0.28   ModalVoice peaks ≈ 1.09 raw (tonal, can sustain into
//                       the bass+snare attack window); 0.28 brings its peak
//                       contribution to ≈ 0.31.
//
// Spec §Reuse from the Existing Codebase suggests starting at
// (bass 6.0, snare 0.9, hi-hat 1.0, resonator 1.0). Those values were
// reference-only — at depth=0.5 they overshoot the peak ≤ 0.95 bound by
// roughly 2x because (a) DaisySP's AnalogSnareDrum and ModalVoice both peak
// above unity in our randomization range and (b) all four voices fire in the
// same control tick during the test. The values above were chosen by sweeping
// candidates against the 8-seed corpus in test_mixer.cpp; they give the
// smallest reduction from the reference that holds the bound for every seed
// while keeping each per-voice contribution well above the 0.05 audibility
// threshold the per-voice presence test asserts.
//
// If a future tweak is needed (new randomization range, new voice), back off
// whichever voice is clipping rather than re-balancing everything, and re-run
// the test_mixer corpus.

#include "Mixer.h"

#include <cstddef>

namespace drum_machine {

Mixer::Mixer(std::array<IInstrument*, kNumVoices> instruments)
    : instruments_(instruments),
      gain_{6.0f, 0.25f, 0.45f, 0.28f} {
}

float Mixer::Process() {
    float sum = 0.0f;
    for (std::size_t i = 0; i < kNumVoices; ++i) {
        sum += gain_[i] * instruments_[i]->Process();
    }
    return sum;
}

void Mixer::SetGain(std::size_t voice, float gain) {
    if (voice < kNumVoices) {
        gain_[voice] = gain;
    }
}

float Mixer::GetGain(std::size_t voice) const {
    return voice < kNumVoices ? gain_[voice] : 0.0f;
}

}  // namespace drum_machine
