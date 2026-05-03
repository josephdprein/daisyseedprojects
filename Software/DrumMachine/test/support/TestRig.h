// TestRig.h — host-test harness wiring DrumMachine to mocks + real instruments.
//
// Per SPECIFICATION.md §Test Harness and tasks/08-drummachine-and-testrig.md:
//   - Constructs 4× MockButton, 4× MockLed, 4× real instruments
//     (BassDrum, Snare, HiHat, Resonator), 1× XorShiftRng, 1× DrumMachine.
//   - Public API:
//       PressButton / ReleaseButton — script edges at the current sim time.
//       HoldButton(pad, ms)         — press now, advance ms, release.
//       AdvanceMs(ms)               — advance the clock in control-tick
//                                     increments (kAudioBlockSize/kSampleRate),
//                                     calling DrumMachine::Tick each tick. No
//                                     audio is computed.
//       CaptureAudio(ms)            — same as AdvanceMs but additionally calls
//                                     DrumMachine::Process() per audio sample.
//       LedBrightness(pad)          — MockLed.Last() for that pad.
//       RandomizationEnabled(pad)   — TestRig-internal mirror of the per-pad
//                                     toggle state (see implementation note).
//       NowMs()                     — current sim time in ms.
//
// MockLed::SetNow(nowMs) is called before every DrumMachine::Tick so the
// brightness samples carry correct timestamps.
//
// Allocation policy: TestRig allocates freely at construction (it owns
// std::vector for CaptureAudio etc.); the audio-rate path is allocation-free
// once the capture buffer is reserved.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "DrumMachine.h"
#include "MockButton.h"
#include "MockLed.h"
#include "PadIndex.h"
#include "instruments/BassDrum.h"
#include "instruments/HiHat.h"
#include "instruments/Resonator.h"
#include "instruments/Snare.h"
#include "randomization/XorShiftRng.h"

class TestRig {
public:
    static constexpr std::size_t kNumPads =
        static_cast<std::size_t>(::PadIndex::Count);

    // Default seed = 1 (XorShiftRng substitutes a fixed non-zero default for 0,
    // but we use 1 explicitly so the seed is meaningful at the call site).
    TestRig();
    explicit TestRig(uint32_t rngSeed);

    void PressButton(::PadIndex pad);
    void HoldButton(::PadIndex pad, uint32_t ms);
    void ReleaseButton(::PadIndex pad);

    // Advance sim clock by `ms` milliseconds in control-tick increments. No
    // audio is computed.
    void AdvanceMs(uint32_t ms);

    // Advance sim clock by `ms` milliseconds AND fill a fresh buffer with one
    // sample per audio frame at Config::kSampleRate. Length is approximately
    // `ms * kSampleRate / 1000` samples (samples accumulate as control ticks
    // are processed; see implementation for the exact mapping).
    std::vector<float> CaptureAudio(uint32_t ms);

    float    LedBrightness(::PadIndex pad) const;
    bool     RandomizationEnabled(::PadIndex pad) const;
    uint32_t NowMs() const { return nowMs_; }

    // Test-only access to the recorded brightness history for a pad's MockLed.
    // Used by EXPECT_LED_FIRED_WITHIN to scan the most-recent N ms.
    const std::vector<std::pair<uint32_t, float>>&
    LedHistory(::PadIndex pad) const;

private:
    // Run a single control tick at the current sim time, optionally appending
    // one block worth of audio samples to `buf` (if buf != nullptr).
    void TickOnce(std::vector<float>* buf);

    static std::size_t Idx(::PadIndex pad) {
        return static_cast<std::size_t>(pad);
    }

    // ---- Mocks owned by the rig --------------------------------------------
    std::array<MockButton, kNumPads> buttons_;
    std::array<MockLed,    kNumPads> leds_;

    // ---- Real instruments + RNG --------------------------------------------
    drum_machine::BassDrum  bass_;
    drum_machine::Snare     snare_;
    drum_machine::HiHat     hihat_;
    drum_machine::Resonator resonator_;
    XorShiftRng             rng_;

    // ---- The engine under test ---------------------------------------------
    drum_machine::DrumMachine machine_;

    // ---- Sim clock + control-tick step in samples --------------------------
    // The control tick advances in increments of (kAudioBlockSize / kSampleRate)
    // seconds. To avoid floating-point drift across long captures we accumulate
    // sub-millisecond fractions in a sample counter and convert to ms only at
    // observable points.
    uint32_t nowMs_       = 0;
    // Sub-ms accumulator: nanoseconds since the last whole-ms boundary, in
    // 1e-9 s units. (Block period at 48 kHz / 48 samples is exactly 1 ms, but
    // the implementer is allowed to retune the block size; the accumulator
    // keeps NowMs() honest in that case.)
    uint64_t subMsNs_     = 0;

    // ---- Observer-mirror of per-pad randomization state --------------------
    // Per task 08 brief, RandomizationEnabled is read from internal bookkeeping
    // the TestRig maintains alongside the engine. Approach #1 (observer): we
    // start at `true` for every pad (matches DrumPad's default) and flip the
    // bit when our own HoldButton helper crosses the threshold. This works
    // because HoldButton is the only path through which the toggle fires in
    // tests — the TestRig owns the buttons and the harness never produces a
    // boot-stuck or multi-event interleave that would diverge from this view.
    std::array<bool, kNumPads> randEnabled_{};
};
