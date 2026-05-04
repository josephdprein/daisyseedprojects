// test_restore_previous.cpp — verifies the press-and-hold-disables-randomization
// flow restores the parameters from the press *before* the hold-press, so the
// locked sound matches the one the user heard just before holding.
//
// The DrumPad pseudocode in SPECIFICATION.md §DrumPad Behavior calls
// IInstrument::RestorePreviousTrig() exactly when randomization transitions
// enabled → disabled. Each instrument keeps a two-deep parameter history,
// shifted at the top of every Trig(); RestorePreviousTrig copies the older
// slot back into current_.
//
// Test layers (one TEST_CASE each unless noted):
//   1. Instrument-level (BassDrum, the only voice with a public Snapshot()):
//        a. Init seeds the history with baseline; calling RestorePreviousTrig
//           before any Trig() leaves params at baseline.
//        b. After (Trig + Randomize) once, RestorePreviousTrig rolls params
//           back to baseline (= the params used by Trig 1).
//        c. After two (Trig + Randomize) cycles, RestorePreviousTrig rolls
//           params back to the params used by Trig 1 — i.e. what the user
//           "heard one press ago" relative to Trig 2.
//        d. After RestorePreviousTrig, a fresh Trig (without Randomize) does
//           not disturb current_ — repeated locked presses keep the same
//           sound.
//   2. DrumPad-level (with StubInstrument):
//        a. The hold-toggle that disables randomization invokes
//           RestorePreviousTrig exactly once, after the press's Trig.
//        b. The hold-toggle that re-enables randomization does NOT call
//           RestorePreviousTrig.
//   3. Engine-level (TestRig with real instruments):
//        a. The user's scenario: press-release, then press-hold-disable;
//           the next free press produces audio identical to the first
//           press's audio (proving the locked sound matches Sound A,
//           not Sound B).

#include "Config.h"
#include "DrumPad.h"
#include "MockButton.h"
#include "MockLed.h"
#include "MockRng.h"
#include "StubInstrument.h"
#include "instruments/BassDrum.h"
#include "test_macros.h"

namespace {

// Drive a DrumPad once per millisecond, mirroring the helper in
// test_hold_toggles.cpp. Kept local so this file has no implicit dependency
// on test/support visibility beyond what the existing tests already use.
void TickAt(drum_machine::DrumPad& pad, MockLed& led, uint32_t nowMs) {
    led.SetNow(nowMs);
    pad.Tick(nowMs);
}

void TickRange(drum_machine::DrumPad& pad, MockLed& led, uint32_t fromMs,
               uint32_t toMs) {
    for (uint32_t t = fromMs; t <= toMs; ++t) {
        TickAt(pad, led, t);
    }
}

// ---- Instrument-level helpers ---------------------------------------------

// Run the per-press cycle the engine performs when randomization is enabled:
// Trig() (which shifts the parameter history internally), then Randomize().
// `seq` injects deterministic per-call uniforms into the MockRng so the test
// can predict the exact resulting param vector if it wants to.
void TrigAndRandomize(drum_machine::BassDrum& bass, MockRng& rng) {
    bass.Trig();
    bass.Randomize(rng);
}

}  // namespace

// ---------------------------------------------------------------------------
// Layer 1: instrument-level behavior of the two-deep history
// ---------------------------------------------------------------------------

TEST_CASE("BassDrum: RestorePreviousTrig before any Trig restores baseline") {
    drum_machine::BassDrum bass;
    bass.Init(48000.0f, /*depth=*/0.5f);

    const auto baseline = bass.Snapshot();   // baselines just applied by Init

    bass.RestorePreviousTrig();

    // History was seeded with baseline at Init, so restoring is a no-op view
    // of current_.
    EXPECT_PARAMS_UNCHANGED(bass.Snapshot(), baseline);
}

TEST_CASE("BassDrum: RestorePreviousTrig after one press restores baseline") {
    drum_machine::BassDrum bass;
    bass.Init(48000.0f, /*depth=*/0.5f);
    const auto baseline = bass.Snapshot();

    MockRng rng(7);
    TrigAndRandomize(bass, rng);
    // current_ is now post-Randomize (different from baseline).
    EXPECT_PARAMS_DIFFER(bass.Snapshot(), baseline);

    // After exactly one Trig(), the older history slot still holds baseline,
    // so RestorePreviousTrig rolls back to baseline — the sound the user
    // heard on press 1.
    bass.RestorePreviousTrig();
    EXPECT_PARAMS_UNCHANGED(bass.Snapshot(), baseline);
}

TEST_CASE("BassDrum: RestorePreviousTrig after two presses restores press-1 params") {
    drum_machine::BassDrum bass;
    bass.Init(48000.0f, /*depth=*/0.5f);

    MockRng rng(11);

    // Press 1: Trig with current_ = baseline (what the user hears as "Sound A"),
    // then Randomize re-rolls current_.
    bass.Trig();
    const auto sound_a_params = bass.Snapshot();  // captured BEFORE Randomize
    bass.Randomize(rng);
    const auto post_press1 = bass.Snapshot();
    EXPECT_PARAMS_DIFFER(post_press1, sound_a_params);

    // Press 2: Trig shifts the history (prev_prev_ ← params from press 1's
    // Trig), voice fires with current_ ("Sound B" — what the user hears on
    // the hold-press), then Randomize re-rolls again.
    bass.Trig();
    const auto sound_b_params = bass.Snapshot();
    bass.Randomize(rng);
    EXPECT_PARAMS_DIFFER(bass.Snapshot(), sound_b_params);

    // The hold-toggle fires here: RestorePreviousTrig must roll back to
    // sound_a_params, NOT sound_b_params.
    bass.RestorePreviousTrig();
    EXPECT_PARAMS_UNCHANGED(bass.Snapshot(), sound_a_params);
    EXPECT_PARAMS_DIFFER(bass.Snapshot(), sound_b_params);
}

TEST_CASE("BassDrum: locked press does not re-shift current_ off the snapshot") {
    drum_machine::BassDrum bass;
    bass.Init(48000.0f, /*depth=*/0.5f);

    MockRng rng(17);
    TrigAndRandomize(bass, rng);
    TrigAndRandomize(bass, rng);
    bass.RestorePreviousTrig();
    const auto locked = bass.Snapshot();

    // Subsequent Trig()s without Randomize() (the engine's behavior when
    // randomization is disabled) must keep producing the locked sound.
    bass.Trig();
    EXPECT_PARAMS_UNCHANGED(bass.Snapshot(), locked);
    bass.Trig();
    EXPECT_PARAMS_UNCHANGED(bass.Snapshot(), locked);
}

// ---------------------------------------------------------------------------
// Layer 2: DrumPad wiring (StubInstrument)
// ---------------------------------------------------------------------------

TEST_CASE("DrumPad: disable hold-toggle calls RestorePreviousTrig exactly once") {
    MockButton     button;
    MockLed        led;
    StubInstrument instrument;
    MockRng        rng(21);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    EXPECT_TRUE(pad.RandomizationEnabled());
    TickAt(pad, led, 0);  // clear first-tick mask flag

    const uint32_t pressAt = 10;
    button.ScriptPress(pressAt);
    button.ScriptRelease(pressAt + Config::kHoldThresholdMs + 50);
    TickRange(pad, led, pressAt, pressAt + Config::kHoldThresholdMs + 80);

    EXPECT_FALSE(pad.RandomizationEnabled());
    EXPECT_EQ(instrument.RestoreCount(), static_cast<std::size_t>(1));

    // Order on the recorded event log: Trig → Randomize → RestorePreviousTrig.
    // (Randomize fired on the press because randomization was still enabled
    // at press time; the toggle disables it AFTER the press handles its
    // synchronous side effects.)
    const auto& events = instrument.Events();
    EXPECT_GE(events.size(), static_cast<std::size_t>(3));
    EXPECT_TRUE(events[0] == StubInstrument::Event::Trig);
    EXPECT_TRUE(events[1] == StubInstrument::Event::Randomize);
    EXPECT_TRUE(events[2] == StubInstrument::Event::RestorePreviousTrig);
}

TEST_CASE("DrumPad: enable hold-toggle does NOT call RestorePreviousTrig") {
    MockButton     button;
    MockLed        led;
    StubInstrument instrument;
    MockRng        rng(22);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    TickAt(pad, led, 0);

    // First hold disables randomization (and triggers one Restore).
    button.ScriptPress(10);
    button.ScriptRelease(10 + Config::kHoldThresholdMs + 50);
    TickRange(pad, led, 10, 10 + Config::kHoldThresholdMs + 80);
    EXPECT_FALSE(pad.RandomizationEnabled());
    EXPECT_EQ(instrument.RestoreCount(), static_cast<std::size_t>(1));

    instrument.Reset();

    // Second hold re-enables randomization. Restore must NOT be called: the
    // user is opting back into variation, not locking another sound.
    const uint32_t secondPress = 10 + Config::kHoldThresholdMs + 200;
    button.ScriptPress(secondPress);
    button.ScriptRelease(secondPress + Config::kHoldThresholdMs + 50);
    TickRange(pad, led, secondPress,
              secondPress + Config::kHoldThresholdMs + 80);

    EXPECT_TRUE(pad.RandomizationEnabled());
    EXPECT_EQ(instrument.RestoreCount(), static_cast<std::size_t>(0));
}

// ---------------------------------------------------------------------------
// Layer 3: end-to-end via DrumPad + real BassDrum (the user's stated scenario)
// ---------------------------------------------------------------------------

// The user's scenario:
//   1. Press and release the bass-drum button → hear Sound A.
//   2. Press and hold the same button → hear Sound B.
//   3. After the 500 ms hold-toggle disables randomization, release.
//   4. Press again → must hear Sound A (the sound from step 1), not Sound B.
//
// We can't compare audio buffers across the sequence sample-for-sample —
// DaisySP voices carry internal phase/oscillator state that evolves with
// every Trig(), so identical parameter sets don't produce identical output
// after different prior-trig histories. Instead we drive the scenario at
// the parameter level: a real BassDrum behind a DrumPad, scripted button
// events, and BassDrum::Snapshot() inspected at the key moments.
//
// This is "end-to-end" in the sense that all the moving parts are real:
// DrumPad's full Tick state machine, the actual instrument's two-deep
// history shift inside Trig(), the actual RestorePreviousTrig() applied to
// the DaisySP setters. Only the audio rendering is skipped.
TEST_CASE("End-to-end: locked press params match the first press params") {
    MockButton             button;
    MockLed                led;
    drum_machine::BassDrum bass;
    MockRng                rng(/*seed=*/4242);
    drum_machine::DrumPad  pad(button, led, bass, rng);

    bass.Init(48000.0f, /*depth=*/0.5f);
    const auto sound_a_params = bass.Snapshot();   // = baseline at this point

    TickAt(pad, led, 0);  // clear first-tick mask (button still up)

    // Step 1: press-release. The Trig fires with current_ = baseline (Sound A);
    // Randomize then re-rolls current_ for the next press.
    button.ScriptPress(10);
    button.ScriptRelease(15);
    TickRange(pad, led, 10, 20);

    // After step 1, the params live for "the press that just happened" are
    // sound_a_params (baseline). They've been shifted into prev_ inside Trig.
    // current_ is now post-Randomize and differs.
    EXPECT_PARAMS_DIFFER(bass.Snapshot(), sound_a_params);

    // Step 2 + 3: press-and-hold past the threshold. Inside Tick:
    //   - On JustPressed: Trig (Sound B fires with the post-step-1 current_),
    //     then Randomize re-rolls current_ again.
    //   - At t = press + kHoldThresholdMs: hold-toggle disables randomization
    //     and calls RestorePreviousTrig, which copies prev_prev_ (= the
    //     params used for step 1's Trig = sound_a_params) back into current_.
    const uint32_t kHoldStart = 100;
    const uint32_t kHoldEnd   = kHoldStart + Config::kHoldThresholdMs + 50;
    button.ScriptPress(kHoldStart);
    button.ScriptRelease(kHoldEnd);
    TickRange(pad, led, kHoldStart, kHoldEnd + 5);

    EXPECT_FALSE(pad.RandomizationEnabled());
    // The locked sound — current_ after RestorePreviousTrig — must match the
    // params the user heard on step 1's press (sound_a_params), NOT the
    // post-step-1-Randomize values that produced Sound B.
    EXPECT_PARAMS_UNCHANGED(bass.Snapshot(), sound_a_params);

    // Step 4: locked press. Trig shifts the history (prev_prev_ = prev_ from
    // step 2; prev_ = current_ = sound_a_params), then voice fires with
    // current_ still equal to sound_a_params. No Randomize because disabled.
    const uint32_t kLockedPress = kHoldEnd + 200;
    button.ScriptPress(kLockedPress);
    button.ScriptRelease(kLockedPress + 5);
    TickRange(pad, led, kLockedPress, kLockedPress + 10);

    // Locked press leaves current_ at sound_a_params (Randomize was skipped).
    EXPECT_PARAMS_UNCHANGED(bass.Snapshot(), sound_a_params);
}
