// RandomizationProfile.h — per-instrument parameter range table.
//
// Per SPECIFICATION.md §Instruments & Randomization. Each instrument owns a
// fixed-capacity table of ParamRange entries describing its randomized
// parameters; depth and the RNG together pick a value per entry.
//
// Container choice (per task 02 brief): std::array<ParamRange, N> templated
// per-instrument, NOT std::vector — no heap in the audio path. Keeping this
// header-only lets task 06 plug in the per-voice `N`s without re-architecting.

#pragma once

#include <array>
#include <cstddef>

// One randomized parameter's [min, max] range plus its baseline (depth=0)
// value. `baseline` may legally fall outside [min, max]; per spec, the final
// computed value is clamp(lerp(baseline, sample, depth), min, max), so the
// clamp tolerates an out-of-range baseline without UB.
struct ParamRange {
    float min;
    float max;
    float baseline;
};

// Fixed-capacity profile. `N` is the number of randomized parameters the
// instrument exposes (e.g. 3 for the bass drum, 4 for the snare). Concrete
// instruments instantiate `RandomizationProfile<N>` and own it by value.
//
// The profile only describes ranges; the *current* parameter values produced
// by Randomize() live on the instrument itself, not here.
template <std::size_t N>
struct RandomizationProfile {
    static constexpr std::size_t kCount = N;
    std::array<ParamRange, N>    params;
};
