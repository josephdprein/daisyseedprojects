// HiHat.cpp — see HiHat.h.

#include "HiHat.h"

#include <algorithm>

namespace drum_machine {

namespace {

inline float ApplyDepth(const ParamRange& range, float u01, float depth) {
    const float sample = range.min + u01 * (range.max - range.min);
    const float raw    = range.baseline + depth * (sample - range.baseline);
    return std::clamp(raw, range.min, range.max);
}

}  // namespace

HiHat::HiHat() {
    profile_.params[0] = ParamRange{5000.0f, 9000.0f, 6000.0f}; // freq Hz
    profile_.params[1] = ParamRange{0.10f,   0.40f,   0.20f};   // decay
    profile_.params[2] = ParamRange{0.50f,   0.90f,   0.70f};   // noisiness
    profile_.params[3] = ParamRange{0.30f,   0.70f,   0.50f};   // tone
}

void HiHat::Init(float sampleRate, float depth01) {
    depth_  = depth01;
    accent_ = 0.7f;

    voice_.Init(sampleRate);

    for (std::size_t i = 0; i < kNumRandomized; ++i) {
        current_[i] = profile_.params[i].baseline;
    }

    voice_.SetFreq(current_[0]);
    voice_.SetDecay(current_[1]);
    voice_.SetNoisiness(current_[2]);
    voice_.SetTone(current_[3]);
    voice_.SetAccent(accent_);
    voice_.SetSustain(false);
}

void HiHat::Trig() {
    voice_.Trig();
}

float HiHat::Process() {
    return voice_.Process(false);
}

void HiHat::Randomize(IRng& rng) {
    for (std::size_t i = 0; i < kNumRandomized; ++i) {
        current_[i] = ApplyDepth(profile_.params[i], rng.NextFloat(), depth_);
    }
    voice_.SetFreq(current_[0]);
    voice_.SetDecay(current_[1]);
    voice_.SetNoisiness(current_[2]);
    voice_.SetTone(current_[3]);
}

}  // namespace drum_machine
