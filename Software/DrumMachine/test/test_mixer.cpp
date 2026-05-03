// test_mixer.cpp — verifies the Mixer's gain-sum behavior end-to-end.
//
// Per tasks/07-mixer.md:
//   1. All four firing → peak ≤ 0.95 across a corpus of RNG seeds (no hard
//      clipping). 500 ms capture so the bass+snare attack overlap is fully
//      contained.
//   2. All four firing → buffer is non-silent.
//   3. Per-voice presence: trigger pads one at a time, capture 200 ms each
//      through the mixer; each must be non-silent. Catches an accidental
//      zero-gain on any one voice.
//
// The instruments are real (BassDrum, Snare, HiHat, Resonator) — same code
// paths that test_instruments.cpp exercises — wired through the real Mixer.
// Only the RNG is mocked, deterministically seeded so the assertions are
// reproducible.

#include "instruments/BassDrum.h"
#include "instruments/HiHat.h"
#include "instruments/Resonator.h"
#include "instruments/Snare.h"
#include "Mixer.h"
#include "MockRng.h"
#include "PadIndex.h"
#include "test_macros.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

constexpr float       kSampleRate    = 48000.0f;
constexpr float       kDepth         = 0.5f;          // spec default
constexpr std::size_t kAllFireSamples = 24000;        // 500 ms @ 48 kHz
constexpr std::size_t kVoiceSamples   = 9600;         // 200 ms @ 48 kHz

// A single rig holding the four voices and the Mixer wired to them.
// Constructed fresh per test case so initial state is deterministic.
struct MixerRig {
    drum_machine::BassDrum  bass;
    drum_machine::Snare     snare;
    drum_machine::HiHat     hihat;
    drum_machine::Resonator reso;
    drum_machine::Mixer     mixer;

    MixerRig()
        : mixer(std::array<IInstrument*, 4>{&bass, &snare, &hihat, &reso}) {
        bass.Init(kSampleRate, kDepth);
        snare.Init(kSampleRate, kDepth);
        hihat.Init(kSampleRate, kDepth);
        reso.Init(kSampleRate, kDepth);
    }

    // Re-roll every voice's parameters from `rng`. Used to vary the per-seed
    // input to the all-four-firing test so the peak bound has to hold for
    // many parameter draws, not just the baselines.
    void RandomizeAll(IRng& rng) {
        bass.Randomize(rng);
        snare.Randomize(rng);
        hihat.Randomize(rng);
        reso.Randomize(rng);
    }

    void TrigAll() {
        bass.Trig();
        snare.Trig();
        hihat.Trig();
        reso.Trig();
    }
};

// Render `samples` mono outputs from the mixer into a fresh vector.
std::vector<float> Render(drum_machine::Mixer& mixer, std::size_t samples) {
    std::vector<float> buf;
    buf.reserve(samples);
    for (std::size_t i = 0; i < samples; ++i) {
        buf.push_back(mixer.Process());
    }
    return buf;
}

}  // namespace

// 1 + 2. All four firing simultaneously. Looped over a corpus of seeds — the
// bound must hold for every seed, not just one. The buffer must also be
// non-silent for each seed (otherwise an accidental gain=0 would silently
// satisfy the upper bound).
TEST_CASE("Mixer: all four firing peak <= 0.95 across seeds") {
    // Mix of arbitrary 32-bit values; selected to exercise different parameter
    // draws. If a future gain change clips on a new seed, expand this list
    // before lowering gains — the bound must hold for any reasonable seed.
    constexpr std::array<uint32_t, 8> kSeeds = {
        0x00000001u, 0xDEADBEEFu, 0xC0FFEEu, 0x12345678u,
        0xA5A5A5A5u, 0x5A5A5A5Au, 0xFEEDFACEu, 0xBADC0DE5u,
    };

    for (uint32_t seed : kSeeds) {
        MixerRig rig;
        MockRng  rng(seed);
        rig.RandomizeAll(rng);
        rig.TrigAll();
        const auto buf = Render(rig.mixer, kAllFireSamples);
        EXPECT_AUDIO_PEAK_LE(buf, 0.95f);
        EXPECT_AUDIO_NOT_SILENT(buf);
    }
}

// 3. Per-voice presence through the mixer. Triggers each pad in isolation —
// the other three voices are silent (untriggered) so any zero-gain on the
// fired voice would land below the 0.05 audibility bar.
TEST_CASE("Mixer: per-voice presence through the summing path") {
    {
        MixerRig rig;
        rig.bass.Trig();
        const auto buf = Render(rig.mixer, kVoiceSamples);
        EXPECT_AUDIO_NOT_SILENT(buf);
    }
    {
        MixerRig rig;
        rig.snare.Trig();
        const auto buf = Render(rig.mixer, kVoiceSamples);
        EXPECT_AUDIO_NOT_SILENT(buf);
    }
    {
        MixerRig rig;
        rig.hihat.Trig();
        const auto buf = Render(rig.mixer, kVoiceSamples);
        EXPECT_AUDIO_NOT_SILENT(buf);
    }
    {
        MixerRig rig;
        rig.reso.Trig();
        const auto buf = Render(rig.mixer, kVoiceSamples);
        EXPECT_AUDIO_NOT_SILENT(buf);
    }
}
