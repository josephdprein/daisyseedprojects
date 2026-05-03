# 04 — RNG implementation and test mocks

## Goal

Provide the deterministic `XorShiftRng` plus the three mock controls
(`MockButton`, `MockLed`, `MockRng`) the rest of the test suite needs.

## Dependencies

- Task 02 (interfaces).

## Files to create

- `src/randomization/XorShiftRng.{h,cpp}`
- `test/support/MockButton.{h,cpp}`
- `test/support/MockLed.{h,cpp}`
- `test/support/MockRng.{h,cpp}`
- `test/test_rng.cpp` *(small, focused — does not appear in spec's required
  test list, but it's worth one or two cases to lock down xorshift semantics)*

## XorShiftRng

- Implements `IRng`. Constructor takes `uint32_t seed`; seed of `0` must not
  collapse the generator (substitute a non-zero default if seed == 0).
- `NextFloat()` returns a value in `[0, 1)` — verify the upper bound is
  exclusive (typical implementation: divide a 24-bit slice by `2^24`).
- Chosen variant: standard 32-bit xorshift `(x ^= x<<13; x ^= x>>17; x ^= x<<5)`.

## MockButton

- Implements `IButton`.
- Internal script: ordered list of `(atMs, pressed: bool)` events.
- `ScriptPress(atMs)` appends `(atMs, true)`; `ScriptRelease(atMs)`
  appends `(atMs, false)`. Multiple events allowed; queued.
- `Update(nowMs)` walks the script, advancing the internal "down" state
  through any events with `atMs <= nowMs` since the last `Update`, and
  computes `JustPressed`/`JustReleased` based on transitions during this
  call. `HeldMs()` returns `0` when not down, otherwise `nowMs - lastPressMs`.
- Out-of-order scripting (`atMs` going backwards) is undefined; document
  that. Tests should never depend on it.

## MockLed

- Implements `ILed`.
- `SetBrightness(v)` clamps `v` to `[0, 1]` and pushes
  `(currentNowMs, clampedValue)` onto an internal vector. `currentNowMs`
  is set externally via a `SetNow(nowMs)` helper used by the test rig
  before each control tick — this is the simplest way to make
  `BrightnessHistory()` carry real timestamps without `MockLed`
  reaching into a global clock.
- `BrightnessHistory()` returns a `const&` to the vector.
- `Last()` returns the most recent value, or `0.0f` if empty.

## MockRng

- Implements `IRng`.
- Default behavior: deterministic xorshift seeded from the constructor's
  `uint32_t`.
- `SetSequence(std::vector<float>)` overrides: `NextFloat()` pops the front
  of the sequence until empty, then falls back to xorshift output. Document
  this fallback so randomization tests don't trip on an exhausted sequence.

## Test cases (`test_rng.cpp`)

1. **Determinism:** two `XorShiftRng(42)` instances produce the same first
   100 values.
2. **Range:** 1000 calls all satisfy `0.0f <= v < 1.0f`.
3. **MockRng sequence:** `SetSequence({0.1f, 0.2f, 0.3f})` — first three
   `NextFloat()` calls return those values in order; fourth call returns
   something in `[0,1)` (xorshift fallback).

## Acceptance criteria

1. All earlier tests still pass; the new RNG tests pass.
2. The mocks compile under `-Wall -Wextra -Werror` and have no public state
   beyond what the spec lists in §Mock APIs (plus the `SetNow` helper noted
   above for `MockLed`).
3. No heap allocation in `XorShiftRng::NextFloat()` (the mocks may allocate
   in their setup methods; `MockLed::SetBrightness` will allocate as the
   history grows — that's accepted because it's test-only).

## Notes / risks

- `MockLed`'s timestamp question is a real design decision. If you prefer
  not to have `SetNow`, the alternative is for the test rig to supply
  timestamps when querying history (e.g.
  `BrightnessHistory()` stores values without timestamps, the rig records
  the `nowMs` it used). Either is fine; keep one and stay consistent.
- `MockButton` should compute `JustPressed`/`JustReleased` such that two
  consecutive `Update` calls with the same `nowMs` both return false (they
  describe transitions during the most recent call, not standing edges).
