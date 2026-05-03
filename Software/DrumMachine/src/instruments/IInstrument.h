// IInstrument.h — the per-voice abstraction owned by a DrumPad.
//
// Per SPECIFICATION.md §Core Interfaces. Concrete implementations wrap
// DaisySP voices (AnalogBassDrum, AnalogSnareDrum, HiHat<>, ModalVoice) and
// are tested via a stub recording Trig/Randomize calls (see spec §Decoupling
// smoke test).
//
// Lifetime: borrowed by reference by DrumPad; owner is main.cpp / TestRig.

#pragma once

#include "../randomization/IRng.h"

class IInstrument {
public:
    virtual ~IInstrument() = default;

    // Initialize the underlying voice and load the baseline parameter set.
    //   sampleRate: forwarded to the DaisySP voice's Init().
    //   depth01:    randomization depth, init-time only.
    //               0 → Randomize() never moves a parameter from baseline.
    //               1 → Randomize() draws each parameter uniformly from
    //                    its [min, max] range.
    // No runtime depth setter (see spec §Instruments & Randomization).
    virtual void Init(float sampleRate, float depth01) = 0;

    // Fire the voice using whatever parameters are currently loaded.
    // The first Trig() after Init() uses the baseline values; each subsequent
    // Randomize() populates the parameters used by the *next* Trig() (see
    // spec §First-press semantics).
    virtual void Trig() = 0;

    // Produce one mono audio sample. Call once per audio frame.
    virtual float Process() = 0;

    // Re-roll randomized parameters for the next Trig(). Each parameter
    // value = clamp(lerp(baseline, min + rng()*(max-min), depth), min, max).
    // Must be no-op-equivalent when depth is 0.
    virtual void Randomize(IRng& rng) = 0;
};
