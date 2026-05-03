# 05 — DrumPad state machine

## Goal

Implement `DrumPad` per spec §DrumPad Behavior. This is the heart of the
firmware and the largest behavioral test surface that doesn't yet require
real instruments.

## Dependencies

- Task 03 (LedTrigger), task 04 (mocks + RNG).

## Files to create

- `src/DrumPad.{h,cpp}`
- `test/support/StubInstrument.h` *(header-only stub `IInstrument` that
  records `Trig`/`Randomize` call counts and the most recent `IRng*` it
  saw — lets `DrumPad` tests run without real DaisySP voices)*
- `test/test_drum_pad.cpp`
- `test/test_hold_toggles.cpp`
- Extend `test/test_led_envelope.cpp` (from task 03) with
  DrumPad-driven cases that exercise the LED through the full pad state
  machine.

## DrumPad responsibilities (verbatim from spec pseudocode)

- Constructor takes `IButton&`, `ILed&`, `IInstrument&`, `IRng&`. All
  borrowed; lifetimes are the caller's problem.
- `Init()` — clears `LedTrigger` state, re-arms hold detection, re-applies
  the boot-stuck-button mask flag (set on the next `Tick`).
- `Tick(uint32_t nowMs)`:
  1. `button.Update(nowMs)`.
  2. **Boot-stuck-button mask:** if this is the first Tick after `Init()`
     and `button.IsDown()`, set `pressMaskedUntilRelease = true`. While
     masked, do nothing except wait for `JustReleased()` to clear the mask.
     Comment the masking site explaining *why*.
  3. On `JustPressed`: `instrument.Trig()`, then `ledTrigger.Fire(nowMs)`,
     then `if (randomizationEnabled) instrument.Randomize(rng)`. Arm
     hold-toggle.
  4. While `IsDown` and hold-toggle armed and `HeldMs() >= kHoldThresholdMs`:
     flip `randomizationEnabled`, `ledTrigger.PulseConfirm(nowMs)`, disarm
     hold-toggle.
  5. On `JustReleased`: disarm hold-toggle.
  6. `led.SetBrightness(ledTrigger.Brightness(nowMs))`.

## Test cases

### `test_drum_pad.cpp`

1. **Trig fires same tick as press:** with `StubInstrument`, script a press
   at t=10 ms; advance to t=10 ms; assert `trigCount == 1`.
2. **Randomize called after Trig:** capture call order in `StubInstrument`
   (e.g. record a list of `enum Event{TRIG, RANDOMIZE}`); assert
   `[TRIG, RANDOMIZE]` in that order on a single press with randomization
   enabled.
3. **Randomize skipped when disabled:** disable randomization (via a hold
   toggle inside the same test), press; assert `randomizeCount` did not
   increase on that press.
4. **Boot-stuck-button mask:** construct `DrumPad`, set `MockButton` script
   so the button is "down at t=0"; first Tick at t=0 — assert no Trig, no
   LED activity, no hold-toggle. Release at t=200 — assert mask clears and
   the *next* press (at t=300) triggers normally.
5. **Decoupling smoke test (per spec):** the pad drives `StubInstrument`
   correctly without including any concrete instrument header.

### `test_hold_toggles.cpp`

1. **500 ms hold flips randomization:** initial state enabled; press at
   t=0, hold through t=600; assert `RandomizationEnabled` is false at
   t=600.
2. **PulseConfirm fires on the toggle:** within the same case, assert
   `MockLed` recorded a brightness pattern matching the PulseConfirm shape
   starting near `kHoldThresholdMs`.
3. **Subsequent presses produce identical params:** with randomization
   disabled, press twice and capture instrument param state from the
   `StubInstrument`'s recorded calls — Randomize was not called between
   presses, so params are identical. (Real audio comparison is in task 08.)
4. **Second 500 ms hold flips back:** continue from previous state; hold
   another 500 ms; assert randomization is enabled again.
5. **Short press does not toggle:** press and release at t=300 (< 500);
   assert randomization state unchanged.
6. **Hold during press triggers exactly once:** press-and-hold for 800 ms
   should have trigCount==1 (hold is not a re-trigger).

### `test_led_envelope.cpp` (additions)

7. **Pad drives LED with Fire on press:** press at t=0; sample
   `MockLed::Last()` at t=2 ms (mid-attack assuming kLedAttackMs=5) and
   assert `> 0`. Confirms the wiring, not the math (math is in tests 1–6).
8. **Pad drives LED with PulseConfirm on hold-toggle:** hold for
   kHoldThresholdMs+1; assert MockLed recorded the pulse pattern.

## Acceptance criteria

1. All test files added or extended above pass.
2. The full suite stays green.
3. `DrumPad` does no heap allocation after construction.
4. The boot-stuck-button mask site has a comment explaining why
   (per spec §DrumPad Behavior).

## Notes / risks

- `Init()` semantics: spec says `DrumMachine::Init` is idempotent and resets
  per-pad state. `DrumPad` itself either has its own `Init()` (preferred)
  or exposes a `Reset()`-equivalent. Match this to whichever approach task
  08's `DrumMachine` will use; either is fine.
- The "first Tick after Init" flag is a single bool in `DrumPad`; flip it
  to false at the end of the first Tick regardless of mask outcome.
- `holdToggleArmed` must be re-armed on every fresh press, not just on
  release. The pseudocode covers this; double-check the implementation.
- "Subsequent presses identical" is hard to assert without a TestRig +
  audio capture. In this task, the assertion is on `StubInstrument` call
  records, not on audio — that's fine and is what the unit test should do.
  Audio-level identity belongs in task 08.
