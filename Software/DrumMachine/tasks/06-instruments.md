# 06 — Instruments + DaisySP wiring

## Goal

Implement the four DaisySP-backed instruments and wire DaisySP source files
into `Makefile.test` so they compile in the host build. This is the first
task that compiles third-party code on the host.

## Dependencies

- Task 02 (interfaces, `RandomizationProfile`).
- Task 04 (`MockRng`).

## Files to create

- `src/instruments/BassDrum.{h,cpp}` — wraps `daisysp::AnalogBassDrum`.
- `src/instruments/Snare.{h,cpp}` — wraps `daisysp::AnalogSnareDrum`.
- `src/instruments/HiHat.{h,cpp}` — wraps `daisysp::HiHat<>`.
- `src/instruments/Resonator.{h,cpp}` — wraps `daisysp::ModalVoice`.
- `test/test_instruments.cpp`
- `test/test_randomization.cpp`
- Add audio assertion helpers to `test/support/test_macros.h`:
  `EXPECT_AUDIO_NOT_SILENT`, `EXPECT_AUDIO_PEAK_LE`, `EXPECT_FIRST_TRANSIENT_WITHIN`,
  `EXPECT_PARAMS_UNCHANGED`, `EXPECT_PARAMS_DIFFER`. Peak = max abs sample.

## Makefile.test changes

- Add include path: `-isystem ../GuitarPedal/dependencies/DaisySP/Source` and
  any additional internal DaisySP include dirs the chosen voices need
  (e.g. `Source/Synthesis/`, `Source/Drums/`, `Source/PhysicalModeling/`,
  `Source/Utility/`, `Source/Effects/` — add as compile errors demand them).
- Compile the minimum DaisySP `.cpp` files needed by the four voices. Add
  files iteratively until the build succeeds; do **not** glob the entire
  DaisySP source tree.
- DaisySP must be included via `-isystem` so its warnings are not promoted
  by the host build's `-Werror`.

## Per-instrument implementation

Each instrument:
- Owns its DaisySP voice as a value member (no heap).
- Owns a `RandomizationProfile` populated at construction with the
  per-parameter `[min, max, baseline]` triples (see spec §Voice-by-voice
  mapping for the param list and baselines; ranges are implementer-chosen).
- `Init(sampleRate, depth)`: stores `depth01`, calls the voice's `Init`,
  applies all baselines via the matching DaisySP setters (this is the
  "first-press semantics" — baseline values are loaded before any
  Randomize).
- `Trig()`: forwards to the voice's `Trig()`.
- `Process()`: returns the voice's per-sample output as `float` mono.
- `Randomize(IRng& rng)`: for each randomized param, compute
  `raw = lerp(baseline, rng.NextFloat() * (max - min) + min, depth)` then
  `value = clamp(raw, min, max)` and call the matching setter.

Per-voice param list (from spec):

| Pad       | Class             | Randomized                                  | Fixed (baseline) |
|-----------|-------------------|---------------------------------------------|------------------|
| Bass      | `AnalogBassDrum`  | freq, decay, tone (`SetSelfFmAmount`)       | accent           |
| Snare     | `AnalogSnareDrum` | freq, decay, snappy, tone                   | accent           |
| Hi-hat    | `HiHat<>`         | freq, decay, noisiness, tone                | accent           |
| Resonator | `ModalVoice`      | freq, structure, brightness, damping        | accent           |

Baselines listed in spec §Voice-by-voice mapping. Decay values are 0..1
normalized per DaisySP (NOT seconds).

## Range tuning (per spec §Range tuning policy)

Tests assert ranges contain randomized values, **not** specific values. Use
musically reasonable starting ranges and accept that final tuning happens
on hardware later. Example anchors (not mandatory):
- Bass freq: `[40, 80] Hz`, decay `[0.3, 0.8]`, tone `[0.0, 0.6]`.
- Snare freq: `[150, 300] Hz`, decay `[0.2, 0.6]`, snappy `[0.4, 0.8]`,
  tone `[0.3, 0.7]`.
- Hi-hat freq: `[5000, 9000] Hz`, decay `[0.1, 0.4]`, noisiness `[0.5, 0.9]`,
  tone `[0.3, 0.7]`.
- Resonator freq: `[150, 400] Hz`, structure `[0.2, 0.6]`,
  brightness `[0.4, 0.8]`, damping `[0.3, 0.7]`.

## Test cases

### `test_instruments.cpp` — once per voice (loop or four cases)

1. **Audible after Trig:** `Init(48000, 0.5)`, `Trig()`, accumulate the
   first 200 ms (`9600` samples) by calling `Process()` per sample,
   `EXPECT_AUDIO_NOT_SILENT(buf)` (peak ≥ 0.05).
2. **Bounded:** same buffer, `EXPECT_AUDIO_PEAK_LE(buf, 1.0f)` (per-voice
   sane bound — final mixer bound is 0.95 in task 07).
3. **First transient within 10 ms:** `EXPECT_FIRST_TRANSIENT_WITHIN(buf, 480)`.
4. **Depth = 0 freezes params:** `Init(48000, 0)`. Capture the post-Init
   parameter state (use a `Snapshot()` helper on each instrument that
   returns `std::array<float, N>` of current setter inputs — add this for
   testing). Call `Randomize(rng)` 5×; assert
   `EXPECT_PARAMS_UNCHANGED(snapshot, currentSnapshot)` after each.
5. **Depth = 1 spans range:** `Init(48000, 1)`. Call `Randomize(rng)` 50×
   collecting each parameter's observed values; assert min observed ≤
   `range.min + 5%` of range, max observed ≥ `range.max - 5%`.
6. **Accent stays at baseline:** snapshot accent before and after 50
   randomizations; `EXPECT_EQ` (or near-equal — exact compare is fine
   since accent is never written).

### `test_randomization.cpp`

1. **Deterministic mapping:** `MockRng::SetSequence({0.0f})` then
   `Randomize` once — assert each param equals `clamp(lerp(baseline, min, depth))`
   exactly (within `1e-5f`).
2. **Sequence at top end:** `SetSequence({0.999999f})` — each param equals
   `clamp(lerp(baseline, just_under_max, depth))`.
3. **Clamp tolerates baseline outside `[min, max]`:** with a deliberately
   pathological profile (one param's baseline outside its range) and depth=0,
   the produced value is clamped into `[min, max]` — no UB, no NaN.

## Acceptance criteria

1. `make -f Makefile.test` compiles all four instruments + the chosen
   DaisySP sources without warnings.
2. New tests pass; full suite stays green.
3. Each instrument is constructed without heap allocation.

## Notes / risks (preflight checks)

- **Verify DaisySP setter names before relying on them:** in particular
  `ModalVoice::SetStructure`, `SetBrightness`, `SetDamping`, `SetAccent`;
  `AnalogBassDrum::SetSelfFmAmount`; `HiHat<>` template default. If a
  setter is missing, surface the discrepancy back to the spec author —
  do not silently substitute.
- The `Snapshot()` helper is test-only scaffolding; a clean way to add it
  is to keep a `std::array<float, N>` "shadow" of last-applied values
  inside each instrument and expose a `const&` accessor under a
  `#if defined(DRUMMACHINE_HOST_TEST)` guard.
- DaisySP voices may need their `Process()` called many samples post-Trig
  before they produce audible output — the 200 ms / 9600-sample buffer
  in the audibility test should be plenty, but if a voice is silent
  spend a moment in a debugger before assuming the test is wrong.
- Adding DaisySP `.cpp` files to `Makefile.test` is iterative — start with
  none, build, copy the missing-symbol error, add the file, repeat. Do
  not list every `.cpp` in DaisySP; that pulls in unrelated voices.
