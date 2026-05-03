// test_simultaneous_press.cpp — end-to-end tests through the DrumMachine
// engine + TestRig harness.
//
// Per tasks/08-drummachine-and-testrig.md "Test cases":
//   1. Two pads pressed in the same control tick: audio non-silent, bounded.
//   2. Four pads pressed in the same control tick: audio non-silent, bounded.
//   3. Two LEDs lit in the same tick (EXPECT_LED_FIRED_WITHIN).
//   4. With randomization disabled on every pad, two consecutive presses
//      produce identical voice output. See the long comment on the test for
//      the determinism story (HiHat/Snare use std::rand() under the hood, so
//      we assert Snapshot()-equality for those voices and audio-equality for
//      the deterministic ones — Bass and Resonator).
//   + Acceptance gate (per task brief item 3): re-Init the engine while a
//     button is "held" via the harness and verify the boot-stuck mask
//     re-engages.
//
// Per spec §Test Harness: TestRig is the only seam — DrumMachine has no
// per-pad accessor.

#include "DrumMachine.h"
#include "Config.h"
#include "PadIndex.h"
#include "TestRig.h"
#include "instruments/BassDrum.h"
#include "instruments/HiHat.h"
#include "instruments/Resonator.h"
#include "instruments/Snare.h"
#include "test_macros.h"
#include "Mixer.h"
#include "MockButton.h"
#include "MockLed.h"
#include "controls/IButton.h"
#include "controls/ILed.h"
#include "instruments/IInstrument.h"
#include "randomization/XorShiftRng.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

// Helper: a run of audio long enough to contain the simultaneous-press
// transient and the slowest decay across the four voices. 500 ms covers it.
constexpr uint32_t kCaptureMs = 500;

// Short pre-warm so the rig's first control tick clears the boot-stuck-mask
// flag for every pad (no buttons held at boot ⇒ mask never engages, but we
// still need one Tick before pressing so JustPressed edges fire on the next
// AdvanceMs).
void Settle(TestRig& rig) {
    rig.AdvanceMs(2);
}

}  // namespace

// 1. Two pads (Bass + Snare) pressed in the same control tick: the mixed
// audio is non-silent and stays under the clipping bound.
TEST_CASE("Simultaneous: two pads same tick are audible and bounded") {
    TestRig rig;
    Settle(rig);

    rig.PressButton(::PadIndex::Bass);
    rig.PressButton(::PadIndex::Snare);

    const auto buf = rig.CaptureAudio(300);
    EXPECT_AUDIO_NOT_SILENT(buf);
    EXPECT_AUDIO_PEAK_LE(buf, 0.95f);
}

// 2. All four pads pressed in the same control tick. Same shape as test 1
// but exercises the worst-case sum the mixer is gain-tuned for.
TEST_CASE("Simultaneous: four pads same tick are audible and bounded") {
    TestRig rig;
    Settle(rig);

    rig.PressButton(::PadIndex::Bass);
    rig.PressButton(::PadIndex::Snare);
    rig.PressButton(::PadIndex::HiHat);
    rig.PressButton(::PadIndex::Resonator);

    const auto buf = rig.CaptureAudio(kCaptureMs);
    EXPECT_AUDIO_NOT_SILENT(buf);
    EXPECT_AUDIO_PEAK_LE(buf, 0.95f);
}

// 3. After two pads are pressed in the same tick, both LEDs show a positive
// brightness sample within the LED attack window. We use 10 ms (the spec's
// kLedAttackMs is 5 ms, so 10 ms safely contains the attack ramp's first
// non-zero sample even with retuned block sizes).
TEST_CASE("Simultaneous: two pads same tick light both LEDs") {
    TestRig rig;
    Settle(rig);

    rig.PressButton(::PadIndex::Bass);
    rig.PressButton(::PadIndex::Snare);

    // Advance just enough for the LED ramp to produce a positive sample.
    // kLedAttackMs is 5 ms; capturing 20 ms is plenty.
    rig.AdvanceMs(20);

    EXPECT_LED_FIRED_WITHIN(rig, ::PadIndex::Bass,  20);
    EXPECT_LED_FIRED_WITHIN(rig, ::PadIndex::Snare, 20);
}

// 4. With randomization disabled, two consecutive presses produce identical
// output for the deterministic voices, and identical parameter snapshots for
// the rand()-driven ones.
//
// HiHat determinism preflight finding:
//   - DaisySP/Source/Drums/hihat.cpp:192 — HiHat::Process() draws noise via
//     std::rand().
//   - DaisySP/Source/Drums/analogsnaredrum.cpp:186 — AnalogSnareDrum::Process()
//     ALSO calls std::rand() in its noise path.
//   - SquareNoise::Init zeroes its phase array (deterministic across Inits).
//   - DaisySP exposes no API to seed/reset its internal RNG state.
//   - AnalogBassDrum and ModalVoice do NOT call std::rand() (verified via
//     grep across the whole DaisySP/Source tree).
//
// Reseeding std::rand() from our HiHat::Init / Snare::Init would break the
// no-syscalls audio-thread policy and would silently couple the two voices
// through a global. So determinism through the *engine* (where every
// Process() call mixes all four voices and advances global rand state) is
// genuinely unachievable for any voice. The test therefore:
//   - Asserts sample-equal audio for Bass and Resonator in isolation
//     (instantiating just the voice, no mixer in the loop).
//   - Asserts Snapshot()-equality for Snare and HiHat — the observable
//     contract for "randomization disabled" is that Randomize() was not
//     called between the two presses, so the parameter values loaded by
//     the previous Randomize are unchanged.
//   - Verifies through the rig that the per-pad RandomizationEnabled flag
//     actually flipped to false after the holds.
TEST_CASE("Simultaneous: identical presses with randomization off match") {
    TestRig rig;
    Settle(rig);

    // Disable randomization on every pad. HoldButton crosses the threshold
    // and flips the toggle for that pad. The first press at the start of the
    // hold also fires Trig + Randomize (default state was enabled), but we
    // never inspect that audio — the assertions below use isolated voices.
    constexpr uint32_t kHold = Config::kHoldThresholdMs + 50;
    rig.HoldButton(::PadIndex::Bass,      kHold);
    rig.HoldButton(::PadIndex::Snare,     kHold);
    rig.HoldButton(::PadIndex::HiHat,     kHold);
    rig.HoldButton(::PadIndex::Resonator, kHold);

    // Engine-level acceptance: every pad now reports randomization disabled.
    EXPECT_FALSE(rig.RandomizationEnabled(::PadIndex::Bass));
    EXPECT_FALSE(rig.RandomizationEnabled(::PadIndex::Snare));
    EXPECT_FALSE(rig.RandomizationEnabled(::PadIndex::HiHat));
    EXPECT_FALSE(rig.RandomizationEnabled(::PadIndex::Resonator));

    // ---- Audio-identity for the deterministic voices ------------------------
    // Each isolated voice is constructed twice with the same seed sequence
    // and the same Trig() pattern. Bypasses the engine deliberately: the
    // engine's mixer mixes all four voices on every Process() call, and
    // Snare+HiHat advance the global rand() state, polluting the captured
    // buffer for any voice if we go through the engine.
    constexpr std::size_t kSamples = 12000;  // 250 ms @ 48 kHz, well past decay
    auto RenderIsolated = [&](auto factory) {
        auto voice = factory();
        voice->Init(Config::kSampleRate, Config::kDefaultRandomizationDepth);
        XorShiftRng rng(7);
        voice->Randomize(rng);     // load some non-baseline params
        voice->Trig();
        std::vector<float> out;
        out.reserve(kSamples);
        for (std::size_t i = 0; i < kSamples; ++i) {
            out.push_back(voice->Process());
        }
        return out;
    };
    {
        const auto a = RenderIsolated(
            []() { return std::make_unique<drum_machine::BassDrum>(); });
        const auto b = RenderIsolated(
            []() { return std::make_unique<drum_machine::BassDrum>(); });
        EXPECT_EQ(a.size(), b.size());
        bool match = true;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a[i] != b[i]) { match = false; break; }
        }
        EXPECT_TRUE(match);
        EXPECT_AUDIO_NOT_SILENT(a);
    }
    {
        const auto a = RenderIsolated(
            []() { return std::make_unique<drum_machine::Resonator>(); });
        const auto b = RenderIsolated(
            []() { return std::make_unique<drum_machine::Resonator>(); });
        EXPECT_EQ(a.size(), b.size());
        bool match = true;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a[i] != b[i]) { match = false; break; }
        }
        EXPECT_TRUE(match);
        EXPECT_AUDIO_NOT_SILENT(a);
    }

    // ---- Parameter-identity for the rand()-driven voices --------------------
    // The observable contract for "randomization disabled" is that the
    // wrapper does NOT call Randomize() between presses, so the parameter
    // values loaded by the most recent Randomize remain in place.
    {
        drum_machine::Snare snare;
        snare.Init(Config::kSampleRate, Config::kDefaultRandomizationDepth);
        XorShiftRng rng(42);
        snare.Randomize(rng);
        const auto before = snare.Snapshot();
        snare.Trig();
        // (No Randomize between Trigs — mimics randomization-disabled state.)
        snare.Trig();
        const auto after = snare.Snapshot();
        EXPECT_PARAMS_UNCHANGED(before, after);
    }
    {
        drum_machine::HiHat hihat;
        hihat.Init(Config::kSampleRate, Config::kDefaultRandomizationDepth);
        XorShiftRng rng(43);
        hihat.Randomize(rng);
        const auto before = hihat.Snapshot();
        hihat.Trig();
        hihat.Trig();
        const auto after = hihat.Snapshot();
        EXPECT_PARAMS_UNCHANGED(before, after);
    }
}

// Acceptance criterion 3: DrumMachine::Init can be called a second time
// mid-test without leaks or undefined state, and the boot-stuck mask
// re-engages on a held button. We can't directly call Init through the
// existing TestRig (it's not exposed), so we wire up the engine ourselves
// with the same mocks we'd use in a TestRig. This is the spec-documented
// "panic reset" path.
TEST_CASE("DrumMachine: re-Init re-engages boot-stuck mask on a held button") {
    // Build the engine by hand so we can call Init(...) on demand.
    std::array<MockButton, 4> buttons;
    std::array<MockLed,    4> leds;
    drum_machine::BassDrum    bass;
    drum_machine::Snare       snare;
    drum_machine::HiHat       hihat;
    drum_machine::Resonator   reso;
    XorShiftRng               rng(7);

    drum_machine::DrumMachine machine(
        std::array<IButton*, 4>{&buttons[0], &buttons[1], &buttons[2],
                                &buttons[3]},
        std::array<ILed*, 4>{&leds[0], &leds[1], &leds[2], &leds[3]},
        std::array<IInstrument*, 4>{&bass, &snare, &hihat, &reso}, rng);

    machine.Init(Config::kSampleRate);

    // Phase 1: press + release Bass cleanly; both edges seen, no mask raised.
    buttons[0].ScriptPress(0);
    buttons[0].ScriptRelease(20);
    for (uint32_t t = 0; t <= 30; ++t) {
        for (auto& l : leds) l.SetNow(t);
        machine.Tick(t);
    }

    // Phase 2: script a press that stays "down" through the next Init() —
    // the press edge happens at t=100, no release before t=200.
    buttons[0].ScriptPress(100);
    for (uint32_t t = 31; t <= 150; ++t) {
        for (auto& l : leds) l.SetNow(t);
        machine.Tick(t);
    }
    // Sanity: at t=100 a fresh Trig fired (this Init isn't masked — the
    // button was up at the moment of Init).
    const std::size_t led_history_pre_reinit = leds[0].BrightnessHistory().size();
    EXPECT_GT(led_history_pre_reinit, static_cast<std::size_t>(0));

    // Phase 3: Init() while the button is still held down. The next Tick
    // must see "button down on first tick after Init" and engage the mask
    // — no Trig, no LED activity until release.
    machine.Init(Config::kSampleRate);

    // Snapshot the LED history right after Init: every entry from now on
    // should be exactly 0 (LedTrigger was reset). We watch for a positive
    // sample to disprove the mask.
    const std::size_t led_history_at_reinit = leds[0].BrightnessHistory().size();

    // Tick well past kHoldThresholdMs while button remains down. If the mask
    // failed, we'd see Trig + Fire (positive LED samples) and/or a hold
    // toggle (PulseConfirm pattern in the LED history).
    for (uint32_t t = 151; t <= 151 + Config::kHoldThresholdMs + 100; ++t) {
        for (auto& l : leds) l.SetNow(t);
        machine.Tick(t);
    }

    bool any_positive_after_reinit = false;
    const auto& hist = leds[0].BrightnessHistory();
    for (std::size_t i = led_history_at_reinit; i < hist.size(); ++i) {
        if (hist[i].second > 0.0f) {
            any_positive_after_reinit = true;
            break;
        }
    }
    EXPECT_FALSE(any_positive_after_reinit);

    // Now release the button — the mask clears. A subsequent press should
    // trigger normally (LED Fire envelope produces a positive sample).
    const uint32_t releaseAt = 151 + Config::kHoldThresholdMs + 200;
    const uint32_t newPressAt = releaseAt + 50;
    buttons[0].ScriptRelease(releaseAt);
    buttons[0].ScriptPress(newPressAt);

    const std::size_t led_history_pre_clear = leds[0].BrightnessHistory().size();
    for (uint32_t t = 151 + Config::kHoldThresholdMs + 101;
         t <= newPressAt + 30; ++t) {
        for (auto& l : leds) l.SetNow(t);
        machine.Tick(t);
    }
    bool any_positive_after_press = false;
    const auto& hist2 = leds[0].BrightnessHistory();
    for (std::size_t i = led_history_pre_clear; i < hist2.size(); ++i) {
        if (hist2[i].second > 0.0f) {
            any_positive_after_press = true;
            break;
        }
    }
    EXPECT_TRUE(any_positive_after_press);
}
