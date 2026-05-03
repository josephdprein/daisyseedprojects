# 07 — Mixer

## Goal

Sum the four instruments into a mono signal with per-voice gains tuned so
all-four-pads-firing peaks ≤ 0.95.

## Dependencies

- Task 06 (instruments).

## Files to create

- `src/Mixer.{h,cpp}`
- `test/test_mixer.cpp`

## Mixer

- Constructor takes `std::array<IInstrument*, 4>` (or four refs — match
  whatever `DrumMachine` will use in task 08; simplest is the `std::array`
  form).
- Per-voice `float gain[4]` array, set at construction (or via a setter
  called once at Init). Default starting values per spec §Reuse:
  bass `× 6.0f`, snare `× 0.9f`, hi-hat `× 1.0f`, resonator `× 1.0f`.
  These are starting values — the test below drives final tuning.
- `Process()` returns
  `gain[0]*inst[0]->Process() + gain[1]*inst[1]->Process() + … `.
- No heap allocation after construction.

## Test cases (`test_mixer.cpp`)

1. **All four firing peak ≤ 0.95:** `Init` all four instruments at default
   sample rate / depth. Trigger all four. Capture 500 ms of audio
   (`Mixer::Process()` per sample) into a buffer.
   `EXPECT_AUDIO_PEAK_LE(buf, 0.95f)` — no hard clipping.
2. **All four firing produce non-silent audio:** same buffer,
   `EXPECT_AUDIO_NOT_SILENT(buf)`.
3. **Per-voice presence (sanity):** trigger pads one at a time, capture
   200 ms each, `EXPECT_AUDIO_NOT_SILENT` per voice through the mixer.
   This catches an accidental zero gain.

## Acceptance criteria

1. `test_mixer.cpp` passes with all-four-firing peak ≤ 0.95.
2. Per-voice peak in isolation ≥ 0.05 (already covered by
   `test_instruments.cpp` task 06 — confirm it still passes after any gain
   adjustments here).
3. Suite stays green.

## Notes / risks

- Gain tuning is iterative. Start at the spec's reference values
  (bass × 6.0, snare × 0.9, hi-hat × 1.0, resonator × 1.0) and back off
  whichever voice is clipping. Document the final values in a comment in
  `Mixer.cpp` so future tweaks have context.
- The peak-≤-0.95 test depends on the random parameters drawn during the
  triggers. Use a fixed RNG seed (the deterministic `XorShiftRng`) to make
  the test reproducible. The test should pass for *any* seed in a
  reasonable corpus — if the test only passes for one seed, the gains
  are still wrong; loop the test over 5–10 seeds and assert the bound
  holds for each.
- Resonator is tonal — it can sustain longer than the other voices and
  alter the peak window. Use a 500 ms capture (not just 100 ms) to cover
  the bass+snare attack overlap.
