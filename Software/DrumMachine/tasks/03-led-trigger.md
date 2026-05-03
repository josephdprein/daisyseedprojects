# 03 — LedTrigger utility

## Goal

Implement and unit-test the `LedTrigger` envelope — the only piece of LED
behavior the rest of the system depends on.

## Dependencies

- Task 02 (Config constants).

## Files to create

- `src/util/LedTrigger.{h,cpp}`
- `test/test_led_envelope.cpp` *(unit tests only — DrumPad-driven LED tests
  arrive in task 05)*

## Behavior (from spec §DrumPad Behavior)

- `Fire(uint32_t nowMs)` schedules a linear ramp:
  - `Brightness(t)` rises linearly `0 → 1` over `[nowMs, nowMs + kLedAttackMs)`.
  - `Brightness(t)` falls linearly `1 → 0` over
    `[nowMs + kLedAttackMs, nowMs + kLedAttackMs + kLedDecayMs)`.
  - After decay completes, brightness is exactly `0`.
- `PulseConfirm(uint32_t nowMs)` plays the pattern
  `on (kPulseConfirmOnMs) → gap (kPulseConfirmGapMs) → on → off`,
  repeated `kPulseConfirmCount` times. The "on" segments hold brightness 1.0,
  the "gap" and trailing segments hold 0.0.
- **PulseConfirm wins:** calling `PulseConfirm` while a `Fire` envelope is
  active cancels the Fire — the pulse pattern alone drives the output.
- `Brightness(uint32_t nowMs)` is a pure function of internal state at `nowMs`;
  there is no implicit time-stepping inside `LedTrigger` (the caller advances
  time).

## Test cases (in `test_led_envelope.cpp`)

1. **Idle:** brightness is `0.0f` before any `Fire`/`PulseConfirm`.
2. **Attack ramp:** after `Fire(1000)`, sample at `nowMs = 1000`, halfway
   through attack, and at the exact peak; assert linear values (use
   `EXPECT_NEAR` with `1e-4f`).
3. **Decay ramp:** sample halfway through decay and at the exact end; assert
   linear values; assert exactly `0.0f` immediately after decay completes.
4. **PulseConfirm shape:** call `PulseConfirm(0)`. Sample inside each `on`
   segment, each `gap`, and after the pattern; assert `1.0f`/`0.0f` accordingly.
   Use the constants from `Config.h` for segment boundaries — the test must
   tolerate any reasonable values, not hard-code 50 ms.
5. **PulseConfirm cancels active Fire:** `Fire(0)`, advance 30 ms, call
   `PulseConfirm(30)`. Assert the next 200 ms of brightness samples match
   the pulse pattern only — no decay tail superimposed.
6. **Re-Fire restarts the envelope:** `Fire(0)`, advance into decay,
   `Fire(50)` — assert brightness immediately drops to the new attack ramp
   from `50`, not a sum.

## Acceptance criteria

1. `test_led_envelope.cpp` adds at least the six cases above and all pass.
2. `make -f Makefile.test && ./build/test/run_all` exits 0.
3. `LedTrigger` does no heap allocation after construction (no
   `std::vector`, no `new`).

## Notes / risks

- Floating-point comparisons must use `EXPECT_NEAR` with a tolerance, not
  `EXPECT_EQ`. The "exactly 0.0f" cases for post-decay are the exception
  and should be exact.
- The pulse pattern implementation can be modeled either as a precomputed
  list of `(end_ms, brightness)` segment boundaries or as on-the-fly
  arithmetic — either is fine; tests don't see the choice.
- Decide whether `Brightness(nowMs)` returning past the end of a pulse
  pattern returns `0.0f` (it should). Add a test case if not already
  covered.
