// BassDrum.cpp — see BassDrum.h.

#include "BassDrum.h"

#include <algorithm>

namespace drum_machine {

namespace {

// clamp(lerp(baseline, sample, depth), min, max). The two-stage formulation
// lets us tolerate baselines that legally fall outside [min, max] (per spec
// §Instruments & Randomization).
inline float ApplyDepth(const ParamRange& range, float u01, float depth) {
    const float sample = range.min + u01 * (range.max - range.min);
    const float raw    = range.baseline + depth * (sample - range.baseline);
    return std::clamp(raw, range.min, range.max);
}

}  // namespace

BassDrum::BassDrum() {
    // Param ranges — first-pass musical defaults; tune on hardware later.
    // Order MUST match the kNumRandomized comment in BassDrum.h.
    profile_.params[0] = ParamRange{40.0f,  80.0f,  50.0f};   // freq Hz
    profile_.params[1] = ParamRange{0.30f,  0.80f,  0.60f};   // decay 0..1
    profile_.params[2] = ParamRange{0.00f,  0.60f,  0.30f};   // tone (SelfFm)
}

void BassDrum::Init(float sampleRate, float depth01) {
    depth_  = depth01;
    accent_ = 0.7f;

    voice_.Init(sampleRate);

    // Apply baselines — first-press semantics: the first Trig() after Init()
    // uses these values. Randomize() repopulates them for subsequent presses.
    current_[0] = profile_.params[0].baseline;
    current_[1] = profile_.params[1].baseline;
    current_[2] = profile_.params[2].baseline;

    voice_.SetFreq(current_[0]);
    voice_.SetDecay(current_[1]);
    voice_.SetSelfFmAmount(current_[2]);
    voice_.SetAccent(accent_);
    voice_.SetSustain(false);
}

void BassDrum::Trig() {
    voice_.Trig();
}

float BassDrum::Process() {
    return voice_.Process(false);
}

void BassDrum::Randomize(IRng& rng) {
    current_[0] = ApplyDepth(profile_.params[0], rng.NextFloat(), depth_);
    current_[1] = ApplyDepth(profile_.params[1], rng.NextFloat(), depth_);
    current_[2] = ApplyDepth(profile_.params[2], rng.NextFloat(), depth_);

    voice_.SetFreq(current_[0]);
    voice_.SetDecay(current_[1]);
    voice_.SetSelfFmAmount(current_[2]);
    // Accent intentionally not re-rolled (see spec).
}

}  // namespace drum_machine
