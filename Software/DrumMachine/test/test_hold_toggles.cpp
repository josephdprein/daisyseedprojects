// test_hold_toggles.cpp — DrumPad press-and-hold randomization-toggle tests.
//
// Per tasks/05-drumpad.md "Test cases" for test_hold_toggles.cpp:
//   1. 500 ms hold flips RandomizationEnabled.
//   2. PulseConfirm fires on the toggle (visible in MockLed history).
//   3. Subsequent presses with randomization disabled record no Randomize().
//   4. A second 500 ms hold flips randomization back on.
//   5. Short press (< 500 ms) does NOT toggle.
//   6. Press-and-hold for 800 ms triggers exactly once (hold is not a
//      re-trigger).
//
// All threshold comparisons use Config::kHoldThresholdMs and the PulseConfirm
// constants — no hard-coded 500/50 ms — so the tests survive a retune.

#include "DrumPad.h"
#include "Config.h"
#include "MockButton.h"
#include "MockLed.h"
#include "MockRng.h"
#include "StubInstrument.h"
#include "test_macros.h"

namespace {

void TickAt(drum_machine::DrumPad& pad, MockLed& led, uint32_t nowMs) {
    led.SetNow(nowMs);
    pad.Tick(nowMs);
}

// Drive `pad` once per millisecond from `fromMs` (inclusive) to `toMs`
// (inclusive).
void TickRange(drum_machine::DrumPad& pad, MockLed& led, uint32_t fromMs,
               uint32_t toMs) {
    for (uint32_t t = fromMs; t <= toMs; ++t) {
        TickAt(pad, led, t);
    }
}

// Returns true iff the MockLed history contains at least one sample equal to
// `value` (within eps) whose timestamp lies in [tMin, tMax].
bool HistoryHasValueInWindow(const MockLed& led, float value, float eps,
                             uint32_t tMin, uint32_t tMax) {
    for (const auto& [ts, v] : led.BrightnessHistory()) {
        if (ts >= tMin && ts <= tMax) {
            const float d = v - value;
            const float a = d < 0 ? -d : d;
            if (a <= eps) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

// 1 + 2: A 500 ms hold flips randomization off AND drives a PulseConfirm
// pattern on the LED starting near the threshold. We assert both within one
// case because the spec ties the two effects together (PulseConfirm IS the
// toggle's LED feedback).
TEST_CASE("DrumPad: 500 ms hold flips randomization and pulses the LED") {
    MockButton      button;
    MockLed         led;
    StubInstrument  instrument;
    MockRng         rng(11);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    EXPECT_TRUE(pad.RandomizationEnabled());

    TickAt(pad, led, 0);  // clear first-tick flag (button still up).

    // Press at t=10, hold past the threshold + the full pulse pattern.
    const uint32_t kPressAt = 10;
    const uint32_t kPulseTotal =
        static_cast<uint32_t>(Config::kPulseConfirmCount) *
        (Config::kPulseConfirmOnMs + Config::kPulseConfirmGapMs);
    const uint32_t kReleaseAt = kPressAt + Config::kHoldThresholdMs +
                                kPulseTotal + 20;
    button.ScriptPress(kPressAt);
    button.ScriptRelease(kReleaseAt);
    TickRange(pad, led, kPressAt, kReleaseAt);

    // Randomization toggled off via the hold-toggle path.
    EXPECT_FALSE(pad.RandomizationEnabled());

    // The press itself triggered exactly one Trig — the hold-toggle does not
    // produce additional Trig calls (covered more directly by case 6 below).
    EXPECT_EQ(instrument.TrigCount(), static_cast<std::size_t>(1));

    // PulseConfirm shape on the LED: at the midpoint of the first "on"
    // segment we expect brightness 1.0; at the midpoint of the first gap we
    // expect 0.0. The pulse starts at the moment the threshold is crossed,
    // which (because we tick once per ms) lands at kPressAt + kHoldThresholdMs.
    const uint32_t pulseStart = kPressAt + Config::kHoldThresholdMs;
    const uint32_t kOn        = Config::kPulseConfirmOnMs;
    const uint32_t kGap       = Config::kPulseConfirmGapMs;
    EXPECT_TRUE(HistoryHasValueInWindow(led, 1.0f, 1e-4f,
                                        pulseStart + kOn / 4,
                                        pulseStart + (3 * kOn) / 4));
    EXPECT_TRUE(HistoryHasValueInWindow(led, 0.0f, 1e-4f,
                                        pulseStart + kOn + kGap / 4,
                                        pulseStart + kOn + (3 * kGap) / 4));
}

// 3. Subsequent presses with randomization disabled record no Randomize() —
//    the spec calls for "subsequent presses produce identical audio buffers";
//    the unit-level proxy is "Randomize was not called between the two
//    presses, so params are identical." Audio identity is in task 08.
TEST_CASE("DrumPad: subsequent presses with randomization off skip Randomize") {
    MockButton      button;
    MockLed         led;
    StubInstrument  instrument;
    MockRng         rng(12);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    TickAt(pad, led, 0);

    // Hold-toggle to disable randomization.
    button.ScriptPress(10);
    button.ScriptRelease(10 + Config::kHoldThresholdMs + 50);
    TickRange(pad, led, 10, 10 + Config::kHoldThresholdMs + 80);
    EXPECT_FALSE(pad.RandomizationEnabled());

    instrument.Reset();

    // Two more short presses, well after the disable.
    const uint32_t base = 10 + Config::kHoldThresholdMs + 200;
    button.ScriptPress(base);
    button.ScriptRelease(base + 5);
    TickRange(pad, led, base, base + 10);

    button.ScriptPress(base + 100);
    button.ScriptRelease(base + 105);
    TickRange(pad, led, base + 100, base + 110);

    EXPECT_EQ(instrument.TrigCount(),      static_cast<std::size_t>(2));
    EXPECT_EQ(instrument.RandomizeCount(), static_cast<std::size_t>(0));
}

// 4. A second 500 ms hold flips randomization back on.
TEST_CASE("DrumPad: second hold flips randomization back on") {
    MockButton      button;
    MockLed         led;
    StubInstrument  instrument;
    MockRng         rng(13);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    TickAt(pad, led, 0);

    // First hold: enabled → disabled.
    button.ScriptPress(10);
    button.ScriptRelease(10 + Config::kHoldThresholdMs + 50);
    TickRange(pad, led, 10, 10 + Config::kHoldThresholdMs + 80);
    EXPECT_FALSE(pad.RandomizationEnabled());

    // Second hold (after a clean release): disabled → enabled.
    const uint32_t secondPress = 10 + Config::kHoldThresholdMs + 200;
    button.ScriptPress(secondPress);
    button.ScriptRelease(secondPress + Config::kHoldThresholdMs + 50);
    TickRange(pad, led, secondPress,
              secondPress + Config::kHoldThresholdMs + 80);
    EXPECT_TRUE(pad.RandomizationEnabled());
}

// 5. Short press (held < kHoldThresholdMs) does NOT flip randomization.
TEST_CASE("DrumPad: short press does not toggle randomization") {
    MockButton      button;
    MockLed         led;
    StubInstrument  instrument;
    MockRng         rng(14);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    EXPECT_TRUE(pad.RandomizationEnabled());

    TickAt(pad, led, 0);

    // Press at t=100, release well before the threshold.
    const uint32_t pressAt   = 100;
    const uint32_t releaseAt = pressAt + (Config::kHoldThresholdMs / 2);
    button.ScriptPress(pressAt);
    button.ScriptRelease(releaseAt);
    TickRange(pad, led, pressAt, releaseAt + 10);

    EXPECT_TRUE(pad.RandomizationEnabled());
    // The press DID Trig (and Randomize, since enabled is the default state) —
    // the toggle path is what's being exercised here, not Trig suppression.
    EXPECT_EQ(instrument.TrigCount(), static_cast<std::size_t>(1));
}

// 6. Press-and-hold for 800 ms triggers exactly once (hold is not a
//    re-trigger). Picks a hold duration comfortably past kHoldThresholdMs
//    without reintroducing the magic number 800.
TEST_CASE("DrumPad: long hold still triggers exactly once") {
    MockButton      button;
    MockLed         led;
    StubInstrument  instrument;
    MockRng         rng(15);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    TickAt(pad, led, 0);

    const uint32_t pressAt   = 10;
    const uint32_t holdMs    = Config::kHoldThresholdMs + 300;  // > threshold.
    const uint32_t releaseAt = pressAt + holdMs;
    button.ScriptPress(pressAt);
    button.ScriptRelease(releaseAt);
    TickRange(pad, led, pressAt, releaseAt + 5);

    EXPECT_EQ(instrument.TrigCount(), static_cast<std::size_t>(1));
}
