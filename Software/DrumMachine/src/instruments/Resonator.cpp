// Resonator.cpp — see Resonator.h.

#include "Resonator.h"

#include <algorithm>

namespace drum_machine {

namespace {

inline float ApplyDepth(const ParamRange& range, float u01, float depth) {
    const float sample = range.min + u01 * (range.max - range.min);
    const float raw    = range.baseline + depth * (sample - range.baseline);
    return std::clamp(raw, range.min, range.max);
}

}  // namespace

Resonator::Resonator() {
    profile_.params[0] = ParamRange{150.0f, 400.0f, 220.0f}; // freq Hz
    profile_.params[1] = ParamRange{0.20f,  0.60f,  0.40f};  // structure
    profile_.params[2] = ParamRange{0.40f,  0.80f,  0.60f};  // brightness
    profile_.params[3] = ParamRange{0.30f,  0.70f,  0.50f};  // damping
}

void Resonator::Init(float sampleRate, float depth01) {
    depth_  = depth01;
    accent_ = 0.7f;

    voice_.Init(sampleRate);

    for (std::size_t i = 0; i < kNumRandomized; ++i) {
        current_[i] = profile_.params[i].baseline;
    }

    voice_.SetFreq(current_[0]);
    voice_.SetStructure(current_[1]);
    voice_.SetBrightness(current_[2]);
    voice_.SetDamping(current_[3]);
    voice_.SetAccent(accent_);
    voice_.SetSustain(false);
}

void Resonator::Trig() {
    voice_.Trig();
}

float Resonator::Process() {
    return voice_.Process(false);
}

void Resonator::Randomize(IRng& rng) {
    for (std::size_t i = 0; i < kNumRandomized; ++i) {
        current_[i] = ApplyDepth(profile_.params[i], rng.NextFloat(), depth_);
    }
    voice_.SetFreq(current_[0]);
    voice_.SetStructure(current_[1]);
    voice_.SetBrightness(current_[2]);
    voice_.SetDamping(current_[3]);
}

}  // namespace drum_machine
