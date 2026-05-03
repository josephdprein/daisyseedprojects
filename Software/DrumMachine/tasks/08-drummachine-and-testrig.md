# 08 — DrumMachine engine + TestRig

## Goal

Wire the four `DrumPad`s and the `Mixer` together into the public
`DrumMachine` API. Build the `TestRig` test harness that the simultaneous
press tests and any future end-to-end tests use.

## Dependencies

- Task 05 (DrumPad), task 06 (instruments), task 07 (Mixer).

## Files to create

- `src/DrumMachine.{h,cpp}`
- `test/support/TestRig.{h,cpp}`
- `test/test_simultaneous_press.cpp`

## DrumMachine

Per spec §DrumMachine API:

- Constructor takes `std::array<IButton*, 4>`, `std::array<ILed*, 4>`,
  `std::array<IInstrument*, 4>`, and `IRng&`. All borrowed.
- `Init(float sampleRate)` is **idempotent**:
  - For each pad, calls `instrument->Init(sampleRate, Config::kDefaultRandomizationDepth)`.
  - Resets every `DrumPad` state: clears `LedTrigger`, re-arms hold detection,
    sets `firstTickAfterInit = true` (so the boot-stuck mask re-applies).
- `Tick(uint32_t nowMs)` calls each pad's `Tick(nowMs)` in `PadIndex` order.
- `Process()` returns one mono sample from `Mixer::Process()`.
- No public per-pad accessor (per spec).

## TestRig

The test seam. Constructs:
- 4 `MockButton`s
- 4 `MockLed`s
- 4 real instruments (`BassDrum`, `Snare`, `HiHat`, `Resonator`)
- 1 `XorShiftRng` (seeded from constructor arg; default seed 1)
- 1 `DrumMachine` wired to the above.

Public API per spec:

```cpp
class TestRig {
public:
    TestRig();
    explicit TestRig(uint32_t rngSeed);

    void PressButton(PadIndex pad);
    void HoldButton(PadIndex pad, uint32_t ms);
    void ReleaseButton(PadIndex pad);
    void AdvanceMs(uint32_t ms);
    std::vector<float> CaptureAudio(uint32_t ms);

    float    LedBrightness(PadIndex pad) const;
    bool     RandomizationEnabled(PadIndex pad) const;
    uint32_t NowMs() const;
};
```

### Implementation notes

- Internal sim clock starts at 0. `AdvanceMs(n)` advances the clock in
  control-tick increments matching `Config::kAudioBlockSize / kSampleRate`,
  calling `MockLed::SetNow(nowMs)` then `DrumMachine::Tick(nowMs)` each tick.
  No audio is computed.
- `CaptureAudio(n)` advances the clock the same way **and** calls
  `DrumMachine::Process()` per audio sample, accumulating samples into the
  returned `std::vector<float>`. Length is `n * kSampleRate / 1000` samples.
- `PressButton(pad)`: scripts a press via `MockButton::ScriptPress(NowMs())`.
  Holds the button down for at least one control tick (the next `AdvanceMs`
  or `CaptureAudio` will tick the engine), then auto-schedules a release a
  little later — actually, **don't** auto-release: leave that to the test.
  `PressButton` is "press now," `ReleaseButton(pad)` is "release now."
- `HoldButton(pad, ms)`: press at `NowMs()`, `AdvanceMs(ms)`, release at
  the new `NowMs()`. This is a convenience for hold-toggle tests.

### `RandomizationEnabled` — design choice

Spec §Test Harness says "RandomizationEnabled and LedBrightness are read by
the TestRig from the MockLed and from internal bookkeeping it maintains
alongside the engine."

**Recommended implementation:** TestRig mirrors the toggle state internally
by watching its own press/release scripting — every time it scripts a
press whose duration ends up ≥ `kHoldThresholdMs`, it flips a local bool.
Initial state is `true`, matching the engine's default. This is fragile
(it duplicates pad logic) but is what the spec asks for; the alternative
— a `friend class TestRig` declaration on `DrumPad` — requires touching
production code and is explicitly avoided by the spec's wording.

If during implementation the duplication is genuinely too brittle (e.g.
edge cases where the boot-stuck mask suppresses a hold-toggle), surface
this and propose adding a friend declaration. **Do not add the friend
declaration without raising it first.**

`LedBrightness(pad)`: just `MockLed[pad].Last()` — no duplication needed.

## Test cases (`test_simultaneous_press.cpp`)

1. **Two pads same tick:** `PressButton(Bass); PressButton(Snare);` then
   `CaptureAudio(300)`; `EXPECT_AUDIO_NOT_SILENT`,
   `EXPECT_AUDIO_PEAK_LE(0.95)`.
2. **Four pads same tick:** all four pressed; `CaptureAudio(500)`;
   `EXPECT_AUDIO_NOT_SILENT`, `EXPECT_AUDIO_PEAK_LE(0.95)`.
3. **Two LEDs lit same tick:** after the press,
   `EXPECT_LED_FIRED_WITHIN(rig, Bass, 10)` and same for Snare.
4. **Subsequent identical presses with randomization disabled produce
   identical audio:** hold each pad >500 ms to disable randomization;
   `CaptureAudio` of two consecutive presses (with enough silence between
   to fully decay) — assert the buffers are sample-equal (or near-equal
   with `EXPECT_NEAR` tolerance for FP determinism if the DaisySP voices
   contain any non-deterministic noise generators that aren't reset by
   `Init`). If they aren't sample-equal, that is a real spec gap to surface.

Add `EXPECT_LED_FIRED_WITHIN` to `test_macros.h` if not already there from
task 06: peeks at the most recent N ms of MockLed history and asserts at
least one sample > 0.

## Acceptance criteria

1. `test_simultaneous_press.cpp` passes.
2. Full suite green.
3. `DrumMachine::Init(sampleRate)` is callable a second time mid-test
   without leaks or undefined state — add a small case in
   `test_drum_pad.cpp` or here that re-Inits the engine and verifies the
   boot-stuck mask re-engages on a held button.

## Notes / risks

- The "subsequent identical presses produce identical audio" test (item 4)
  is the deepest behavioral check in the suite. If a DaisySP voice
  internally uses an uncoupled noise generator that's not deterministic
  per `Init`, two identical-parameter triggers could still produce
  different audio. Investigate before weakening the assertion. This is
  worth a `#TODO` or a flagged note back to the spec author if it's the
  case for HiHat (which is noise-driven).
- TestRig owning real instruments means task 06 must already be merged.
- TestRig's `AdvanceMs` should advance in increments of one control tick
  (block size / sample rate) — not in 1 ms steps unless the block size
  happens to be 48. Tests should not assume 1 ms tick granularity.
