// test_drum_pad.cpp — unit tests for src/DrumPad.cpp.
//
// Per tasks/05-drumpad.md "Test cases":
//   1. Trig fires same tick as press.
//   2. Randomize is called *after* Trig (event-order check).
//   3. Randomize skipped when randomization is disabled.
//   4. Boot-stuck-button mask: button held at first Tick is suppressed until
//      released; the next press then triggers normally.
//   5. Decoupling smoke test: pad drives StubInstrument without including any
//      concrete instrument header (this whole file deliberately includes only
//      the IInstrument interface via StubInstrument.h).
//
// Notes on test wiring:
//   - DrumPad's first Tick after construction (or after Init()) checks
//     button.IsDown() and, if true, sets the boot-stuck mask. To exercise the
//     "press at t=N triggers" cases we run a dummy Tick at an earlier time
//     when the button is not yet down so the first-tick flag clears without
//     the mask being raised.
//   - MockLed::SetNow(nowMs) is called before each pad Tick so the recorded
//     brightness samples carry the right timestamp (DrumPad calls
//     led.SetBrightness once per Tick at the end).

#include "DrumPad.h"
#include "Config.h"
#include "MockButton.h"
#include "MockLed.h"
#include "MockRng.h"
#include "StubInstrument.h"
#include "test_macros.h"

namespace {

// Helper: drive a DrumPad through a single Tick at nowMs, threading the time
// through MockLed so its history entries carry the correct timestamp.
void TickAt(drum_machine::DrumPad& pad, MockLed& led, uint32_t nowMs) {
    led.SetNow(nowMs);
    pad.Tick(nowMs);
}

}  // namespace

// 1. Press at t=10 ms produces exactly one Trig() call in the same tick.
TEST_CASE("DrumPad: Trig fires same tick as press") {
    MockButton      button;
    MockLed         led;
    StubInstrument  instrument;
    MockRng         rng(1);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    // Clear the first-tick-after-init flag with a no-op Tick before any press
    // so we don't accidentally trip the boot-stuck-button mask.
    TickAt(pad, led, 0);

    button.ScriptPress(10);
    TickAt(pad, led, 10);

    EXPECT_EQ(instrument.TrigCount(), static_cast<std::size_t>(1));
}

// 2. Trig is recorded BEFORE Randomize on a single press with randomization
//    enabled — the spec requires "trigger immediately, then re-randomize for
//    the next press."
TEST_CASE("DrumPad: Randomize is called after Trig on a single press") {
    MockButton      button;
    MockLed         led;
    StubInstrument  instrument;
    MockRng         rng(2);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    TickAt(pad, led, 0);  // clear first-tick flag (button up).

    button.ScriptPress(5);
    TickAt(pad, led, 5);

    const auto& events = instrument.Events();
    EXPECT_EQ(events.size(), static_cast<std::size_t>(2));
    EXPECT_TRUE(events[0] == StubInstrument::Event::Trig);
    EXPECT_TRUE(events[1] == StubInstrument::Event::Randomize);

    // The IRng* captured by Randomize() must be the one we passed to the
    // constructor — proves the wiring without inspecting any concrete RNG.
    EXPECT_EQ(instrument.LastRng(), static_cast<IRng*>(&rng));
}

// 3. After a hold-toggle disables randomization, the next press still calls
//    Trig but does NOT call Randomize.
TEST_CASE("DrumPad: Randomize skipped when randomization is disabled") {
    MockButton      button;
    MockLed         led;
    StubInstrument  instrument;
    MockRng         rng(3);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    EXPECT_TRUE(pad.RandomizationEnabled());  // sanity: default state is on.

    TickAt(pad, led, 0);  // clear first-tick flag.

    // Press, hold past the threshold, release — this flips randomization off
    // via the PulseConfirm hold-toggle path. We tick once per millisecond so
    // the held-time check inside DrumPad sees the threshold cross.
    button.ScriptPress(10);
    button.ScriptRelease(10 + Config::kHoldThresholdMs + 50);
    for (uint32_t t = 1; t <= 10 + Config::kHoldThresholdMs + 60; ++t) {
        TickAt(pad, led, t);
    }
    EXPECT_FALSE(pad.RandomizationEnabled());

    // Reset the stub so the next press's events are easy to inspect.
    instrument.Reset();

    // Now a fresh press: Trig fires, Randomize must NOT fire.
    const uint32_t pressAt = 10 + Config::kHoldThresholdMs + 200;
    button.ScriptPress(pressAt);
    button.ScriptRelease(pressAt + 5);
    for (uint32_t t = pressAt; t <= pressAt + 10; ++t) {
        TickAt(pad, led, t);
    }

    EXPECT_EQ(instrument.TrigCount(),      static_cast<std::size_t>(1));
    EXPECT_EQ(instrument.RandomizeCount(), static_cast<std::size_t>(0));
}

// 4. Boot-stuck-button mask: button is "down" at t=0, the very first Tick.
//    Per spec: no Trig, no LED activity, no hold-toggle while masked. After
//    the button is observed released, the mask clears and a fresh press
//    triggers normally.
TEST_CASE("DrumPad: boot-stuck button is masked until released") {
    MockButton      button;
    MockLed         led;
    StubInstrument  instrument;
    MockRng         rng(4);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    // Script: button is held down from t=0 (the initial "stuck" condition),
    // released at t=200, then pressed again at t=300.
    button.ScriptPress(0);
    button.ScriptRelease(200);
    button.ScriptPress(300);

    // Tick across the masked window — long enough that, if the mask were
    // broken, the hold-toggle path would also have fired (i.e. > kHold).
    for (uint32_t t = 0; t <= 199; ++t) {
        TickAt(pad, led, t);
    }
    EXPECT_EQ(instrument.TrigCount(),      static_cast<std::size_t>(0));
    EXPECT_EQ(instrument.RandomizeCount(), static_cast<std::size_t>(0));
    // Randomization must not have toggled either — the hold-toggle path is
    // disabled while masked.
    EXPECT_TRUE(pad.RandomizationEnabled());
    // LED must be dark for every recorded sample so far (LedTrigger was never
    // fired and PulseConfirm was never called).
    for (const auto& [ts, val] : led.BrightnessHistory()) {
        (void)ts;
        EXPECT_NEAR(val, 0.0f, 1e-6f);
    }

    // Release at t=200 clears the mask.
    TickAt(pad, led, 200);
    // No Trig from the release itself.
    EXPECT_EQ(instrument.TrigCount(), static_cast<std::size_t>(0));

    // Tick across the gap (button up) — still no Trig.
    for (uint32_t t = 201; t <= 299; ++t) {
        TickAt(pad, led, t);
    }
    EXPECT_EQ(instrument.TrigCount(), static_cast<std::size_t>(0));

    // Press at t=300 — now masked is cleared, this is a real press.
    TickAt(pad, led, 300);
    EXPECT_EQ(instrument.TrigCount(),      static_cast<std::size_t>(1));
    EXPECT_EQ(instrument.RandomizeCount(), static_cast<std::size_t>(1));
}

// 5. Decoupling smoke test (per spec §Decoupling smoke test): the pad drives
//    a StubInstrument correctly without depending on any concrete instrument
//    header. The fact that this whole file compiles and links without ever
//    including BassDrum.h / Snare.h / HiHat.h / Resonator.h is the structural
//    half of the assertion; the behavioral half is below.
TEST_CASE("DrumPad: drives the stub instrument with no concrete deps") {
    MockButton      button;
    MockLed         led;
    StubInstrument  instrument;
    MockRng         rng(5);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    TickAt(pad, led, 0);  // clear first-tick flag.

    // Two presses, each followed by a release. With randomization enabled
    // (default), each press should produce one Trig and one Randomize.
    for (int i = 0; i < 2; ++i) {
        const uint32_t pressAt = static_cast<uint32_t>(20 + i * 50);
        button.ScriptPress(pressAt);
        button.ScriptRelease(pressAt + 10);
        for (uint32_t t = pressAt; t <= pressAt + 15; ++t) {
            TickAt(pad, led, t);
        }
    }

    EXPECT_EQ(instrument.TrigCount(),      static_cast<std::size_t>(2));
    EXPECT_EQ(instrument.RandomizeCount(), static_cast<std::size_t>(2));
    EXPECT_EQ(instrument.LastRng(),        static_cast<IRng*>(&rng));
}
