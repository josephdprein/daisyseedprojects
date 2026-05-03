// test_led_envelope.cpp — unit tests for src/util/LedTrigger.
//
// Per tasks/03-led-trigger.md acceptance criteria. Six required cases:
//   1. Idle:                 brightness 0 before any Fire/PulseConfirm.
//   2. Attack ramp:          linear 0 → 1 across kLedAttackMs.
//   3. Decay ramp:           linear 1 → 0 across kLedDecayMs; exactly 0 after.
//   4. PulseConfirm shape:   on/gap/on/.../off matches Config constants.
//   5. PulseConfirm cancels: PulseConfirm during active Fire wins.
//   6. Re-Fire restarts:     second Fire replaces in-flight envelope.
//
// Plus a small "past-pattern returns 0" case noted in the task brief's
// "Notes / risks".
//
// Task 05 additions (DrumPad-driven LED-wiring tests):
//   7. Pad press lights the LED via MockLed (Fire path).
//   8. Pad hold-toggle drives the PulseConfirm pattern via MockLed.
//
// Per task brief: never hard-code 50/120 ms; always use Config constants so
// the tests survive any reasonable retune of the timings.

#include "Config.h"
#include "DrumPad.h"
#include "MockButton.h"
#include "MockLed.h"
#include "MockRng.h"
#include "StubInstrument.h"
#include "test_macros.h"

namespace {

constexpr float kEps = 1e-4f;

}  // namespace

TEST_CASE("LedTrigger idle brightness is zero") {
    drum_machine::LedTrigger trig;
    // Sample at several arbitrary times before any Fire/PulseConfirm — the
    // result must be exactly 0 in every case (no envelope active).
    EXPECT_NEAR(trig.Brightness(0), 0.0f, kEps);
    EXPECT_NEAR(trig.Brightness(123), 0.0f, kEps);
    EXPECT_NEAR(trig.Brightness(1'000'000), 0.0f, kEps);
}

TEST_CASE("LedTrigger Fire produces linear attack ramp") {
    drum_machine::LedTrigger trig;
    constexpr uint32_t       kStart = 1000;
    trig.Fire(kStart);

    // At t = start, attack just beginning → 0.0.
    EXPECT_NEAR(trig.Brightness(kStart), 0.0f, kEps);

    // Halfway through attack: linear interpolation gives 0.5. We use integer
    // halving — for kLedAttackMs even this is exact; for odd values the
    // tolerance still covers the rounding step (1/kLedAttackMs).
    const uint32_t halfAttack = Config::kLedAttackMs / 2;
    const float    expectedHalf =
        static_cast<float>(halfAttack) /
        static_cast<float>(Config::kLedAttackMs);
    EXPECT_NEAR(trig.Brightness(kStart + halfAttack), expectedHalf, kEps);

    // Exact peak: at t = start + kLedAttackMs we are at the boundary between
    // attack-end and decay-start. Per spec the rising ramp goes "0 → 1 over
    // [start, start + kAttack)", and the decay then goes "1 → 0 over
    // [start + kAttack, start + kAttack + kDecay)". So the value at exactly
    // start + kAttack is 1.0 (start of decay).
    EXPECT_NEAR(trig.Brightness(kStart + Config::kLedAttackMs), 1.0f, kEps);
}

TEST_CASE("LedTrigger Fire produces linear decay ramp and post-decay zero") {
    drum_machine::LedTrigger trig;
    constexpr uint32_t       kStart = 0;
    trig.Fire(kStart);

    const uint32_t decayStart = Config::kLedAttackMs;
    const uint32_t decayEnd   = Config::kLedAttackMs + Config::kLedDecayMs;

    // Halfway through decay: 1 - 0.5 = 0.5.
    const uint32_t halfDecay     = Config::kLedDecayMs / 2;
    const float    expectedHalf  = 1.0f - static_cast<float>(halfDecay) /
                                              static_cast<float>(Config::kLedDecayMs);
    EXPECT_NEAR(trig.Brightness(kStart + decayStart + halfDecay),
                expectedHalf, kEps);

    // One ms before decay end (still inside the decay window): brightness is
    // very close to 0 but not yet exactly 0.
    const float expectedNearEnd =
        1.0f - static_cast<float>(Config::kLedDecayMs - 1) /
                   static_cast<float>(Config::kLedDecayMs);
    EXPECT_NEAR(trig.Brightness(kStart + decayEnd - 1), expectedNearEnd,
                kEps);

    // At exact decay end and beyond: spec says "exactly 0". Use EXPECT_NEAR
    // with zero tolerance — the implementation returns a literal 0.0f here.
    EXPECT_NEAR(trig.Brightness(kStart + decayEnd), 0.0f, 0.0f);
    EXPECT_NEAR(trig.Brightness(kStart + decayEnd + 50), 0.0f, 0.0f);
    EXPECT_NEAR(trig.Brightness(kStart + decayEnd + 10000), 0.0f, 0.0f);
}

TEST_CASE("LedTrigger PulseConfirm shape matches Config constants") {
    drum_machine::LedTrigger trig;
    trig.PulseConfirm(0);

    const uint32_t kOn   = Config::kPulseConfirmOnMs;
    const uint32_t kGap  = Config::kPulseConfirmGapMs;
    const uint32_t kUnit = kOn + kGap;
    const uint32_t kCount = static_cast<uint32_t>(Config::kPulseConfirmCount);

    // Walk every (on, gap) pair: midpoint of each on-segment is 1.0; midpoint
    // of each gap is 0.0. Use Config constants so the test tolerates retunes.
    for (uint32_t i = 0; i < kCount; ++i) {
        const uint32_t unitStart = i * kUnit;
        // Midpoint of "on" segment.
        EXPECT_NEAR(trig.Brightness(unitStart + kOn / 2), 1.0f, kEps);
        // Just after the on→gap boundary.
        EXPECT_NEAR(trig.Brightness(unitStart + kOn), 0.0f, kEps);
        // Midpoint of "gap" segment.
        EXPECT_NEAR(trig.Brightness(unitStart + kOn + kGap / 2), 0.0f, kEps);
    }

    // After the pattern completes: 0.0 forever.
    const uint32_t total = kCount * kUnit;
    EXPECT_NEAR(trig.Brightness(total), 0.0f, kEps);
    EXPECT_NEAR(trig.Brightness(total + 1), 0.0f, kEps);
    EXPECT_NEAR(trig.Brightness(total + 1000), 0.0f, kEps);
}

TEST_CASE("LedTrigger PulseConfirm cancels active Fire envelope") {
    drum_machine::LedTrigger trig;

    trig.Fire(0);
    // Advance 30 ms (somewhere inside the decay region for default constants;
    // for very small kLedDecayMs this would still leave us in the post-decay
    // tail — either way the behavior under test is the same: PulseConfirm
    // overrides whatever Fire would have produced).
    constexpr uint32_t kPulseStart = 30;
    trig.PulseConfirm(kPulseStart);

    const uint32_t kOn    = Config::kPulseConfirmOnMs;
    const uint32_t kGap   = Config::kPulseConfirmGapMs;
    const uint32_t kUnit  = kOn + kGap;
    const uint32_t kCount = static_cast<uint32_t>(Config::kPulseConfirmCount);
    const uint32_t total  = kCount * kUnit;

    // Sample the next 200 ms (or pattern length, whichever is longer) and
    // confirm the value matches the pulse pattern only — no decay tail
    // superimposed. The pulse output is binary 0/1, so any leakage from a
    // decaying Fire would show up as a non-{0,1} value.
    const uint32_t kHorizon = 200;
    for (uint32_t dt = 0; dt < kHorizon; ++dt) {
        const float    b      = trig.Brightness(kPulseStart + dt);
        const float    expect =
            (dt < total)
                ? ((dt % kUnit) < kOn ? 1.0f : 0.0f)
                : 0.0f;
        EXPECT_NEAR(b, expect, kEps);
    }
}

TEST_CASE("LedTrigger second Fire restarts the envelope") {
    drum_machine::LedTrigger trig;

    trig.Fire(0);
    // Advance into the decay region.
    const uint32_t midDecay = Config::kLedAttackMs + Config::kLedDecayMs / 2;
    // (Sanity: midDecay > kLedAttackMs by definition for kLedDecayMs >= 2.)
    EXPECT_GT(trig.Brightness(midDecay), 0.0f);

    // Re-Fire at a new time — envelope must restart from this new moment, not
    // sum with whatever was decaying.
    constexpr uint32_t kReFire = 50;
    trig.Fire(kReFire);

    // Immediately after the new Fire, brightness is at the start of the new
    // attack ramp (== 0.0), regardless of what the old envelope had produced.
    EXPECT_NEAR(trig.Brightness(kReFire), 0.0f, kEps);

    // Halfway through the new attack ramp: linear midpoint.
    const uint32_t halfAttack = Config::kLedAttackMs / 2;
    const float    expectedHalf =
        static_cast<float>(halfAttack) /
        static_cast<float>(Config::kLedAttackMs);
    EXPECT_NEAR(trig.Brightness(kReFire + halfAttack), expectedHalf, kEps);

    // Peak of the new envelope at kReFire + kLedAttackMs.
    EXPECT_NEAR(trig.Brightness(kReFire + Config::kLedAttackMs), 1.0f, kEps);
}

// --- Task 05 additions: DrumPad-driven LED wiring tests ------------------

namespace {

// Helper duplicated from test_drum_pad.cpp (header-only sharing isn't worth
// it for two callsites). Drives the pad through one Tick at nowMs while
// threading the timestamp into MockLed so its history is properly tagged.
void PadTickAt(drum_machine::DrumPad& pad, MockLed& led, uint32_t nowMs) {
    led.SetNow(nowMs);
    pad.Tick(nowMs);
}

// Returns true iff the MockLed history contains at least one sample equal to
// `value` (within eps) whose timestamp lies in [tMin, tMax].
bool LedHasValueInWindow(const MockLed& led, float value, float eps,
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

// 7. Pad press lights the LED via MockLed (Fire path). Confirms wiring, not
//    the math (math lives in tests 1–6 above). Per the task 05 brief:
//    "press at t=0; sample MockLed::Last() at t=2 ms (mid-attack assuming
//    kLedAttackMs=5) and assert > 0." We perform a no-op clearing Tick first
//    so the boot-stuck-button mask doesn't fire on the press, and we sample
//    mid-attack relative to the actual press time.
TEST_CASE("DrumPad press lights the LED via MockLed (wiring test)") {
    MockButton            button;
    MockLed               led;
    StubInstrument        instrument;
    MockRng               rng(101);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    // Clear the first-tick-after-init flag with the button still up so the
    // boot-stuck mask is NOT raised when we press below.
    PadTickAt(pad, led, 0);

    // Press the button. Pick a press time and sample time inside the attack
    // ramp using Config::kLedAttackMs so the test survives any retune.
    const uint32_t kPressAt    = 10;
    const uint32_t kAttackMs   = Config::kLedAttackMs;
    // Sample point inside the attack ramp. For kLedAttackMs >= 2 this lands
    // at the midpoint or just below it; for kLedAttackMs == 1 it lands at
    // the peak — still > 0 in both cases.
    const uint32_t kSampleOff  = (kAttackMs > 1) ? kAttackMs / 2 : 1;

    button.ScriptPress(kPressAt);
    // Tick once at the press time so the press is consumed and Fire runs.
    PadTickAt(pad, led, kPressAt);
    // Tick again at the sample point so MockLed::Last() reflects mid-attack.
    PadTickAt(pad, led, kPressAt + kSampleOff);

    EXPECT_GT(led.Last(), 0.0f);
}

// 8. Pad hold-toggle drives the PulseConfirm pattern via MockLed. We hold the
//    button just past the threshold and assert the LED history contains both
//    a 1.0 sample inside the first "on" segment and a 0.0 sample inside the
//    first gap of the pulse pattern that starts at the toggle moment.
TEST_CASE("DrumPad hold-toggle drives PulseConfirm pattern on the LED") {
    MockButton            button;
    MockLed               led;
    StubInstrument        instrument;
    MockRng               rng(102);
    drum_machine::DrumPad pad(button, led, instrument, rng);

    PadTickAt(pad, led, 0);  // clear first-tick flag (button still up).

    const uint32_t kPressAt   = 10;
    const uint32_t kPulseTotal =
        static_cast<uint32_t>(Config::kPulseConfirmCount) *
        (Config::kPulseConfirmOnMs + Config::kPulseConfirmGapMs);
    // Hold long enough that the toggle fires AND the full pulse pattern has
    // had time to play out before release.
    const uint32_t kReleaseAt = kPressAt + Config::kHoldThresholdMs +
                                kPulseTotal + 20;

    button.ScriptPress(kPressAt);
    button.ScriptRelease(kReleaseAt);
    for (uint32_t t = 1; t <= kReleaseAt; ++t) {
        PadTickAt(pad, led, t);
    }

    // The pulse pattern starts at the moment the threshold is crossed.
    // Because we tick once per ms and the press lands at kPressAt, the
    // hold-toggle fires at kPressAt + kHoldThresholdMs.
    const uint32_t pulseStart = kPressAt + Config::kHoldThresholdMs;
    const uint32_t kOn        = Config::kPulseConfirmOnMs;
    const uint32_t kGap       = Config::kPulseConfirmGapMs;

    // Assert the LED was driven HIGH inside the first "on" segment.
    EXPECT_TRUE(LedHasValueInWindow(led, 1.0f, 1e-4f,
                                    pulseStart + kOn / 4,
                                    pulseStart + (3 * kOn) / 4));
    // Assert the LED was driven LOW inside the first gap.
    EXPECT_TRUE(LedHasValueInWindow(led, 0.0f, 1e-4f,
                                    pulseStart + kOn + kGap / 4,
                                    pulseStart + kOn + (3 * kGap) / 4));
}
