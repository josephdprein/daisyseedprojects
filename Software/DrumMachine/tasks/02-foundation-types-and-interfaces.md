# 02 — Foundation types and interfaces

## Goal

Land the type-and-interface skeleton the rest of the codebase compiles
against. No behavioral code; this task is satisfied by "everything compiles
and the existing test suite stays green."

## Dependencies

- Task 01 (scaffold + Makefile.test).

## Files to create

- `src/PadIndex.h`
- `src/Config.h` *(constants only at this point; see §Host build constraint)*
- `src/instruments/IInstrument.h`
- `src/controls/IButton.h`
- `src/controls/ILed.h`
- `src/randomization/IRng.h`
- `src/randomization/RandomizationProfile.h`

## Content per file

Match the declarations in spec §Type Definitions, §Core Interfaces, and
§Hardware Contract verbatim. Specifically:

- `PadIndex` enum class (Bass=0, Snare=1, HiHat=2, Resonator=3, Count=4).
- `IInstrument` — `Init(sampleRate, depth01)`, `Trig`, `Process`, `Randomize(IRng&)`.
- `IButton` — `Update(nowMs)`, `IsDown`, `JustPressed`, `JustReleased`, `HeldMs`.
- `ILed` — `SetBrightness(float v01)`. Implementations clamp.
- `IRng` — `NextFloat()` returns uniform in `[0, 1)`.
- `RandomizationProfile` — `ParamRange{min,max,baseline}` plus a small,
  fixed-capacity container of `ParamRange`s. Use `std::array<ParamRange, N>`
  parameterized per-instrument, **not** `std::vector` (no heap in audio path).
  The exact aggregation shape is the implementer's call; it lands its final
  form in task 06.
- `Config.h` — every `constexpr` constant from spec §Hardware Contract:
  `kSampleRate`, `kAudioBlockSize`, `kHoldThresholdMs`, `kLedAttackMs`,
  `kLedDecayMs`, `kPulseConfirmOnMs`, `kPulseConfirmGapMs`,
  `kPulseConfirmCount`, `kDefaultRandomizationDepth`.

## Host build constraint (the one nuance)

The spec wants `kButtonPins[4]` and `kLedPins[4]` declared in `Config.h` as
`extern const daisy::Pin[4]`. `daisy::Pin` is from libDaisy and is **not
available in the host test build**. Resolve this by gating the pin
declarations:

```cpp
#if !defined(DRUMMACHINE_HOST_TEST)
#include "daisy_seed.h"
namespace Config {
    extern const daisy::Pin kButtonPins[static_cast<size_t>(PadIndex::Count)];
    extern const daisy::Pin kLedPins[static_cast<size_t>(PadIndex::Count)];
}
#endif
```

Add `-DDRUMMACHINE_HOST_TEST` to `Makefile.test`'s `CXXFLAGS` to activate the
gate. This keeps `Config.h` a single file (per spec) while letting it
participate in the host build.

## Acceptance criteria

1. `make -f Makefile.test` still passes; bootstrap test still green.
2. A throwaway `.cpp` that `#include`s every interface header compiles cleanly
   under `-Wall -Wextra -Werror`. (Optional: keep this as `test/test_headers_compile.cpp`.)
3. No `.cpp` file in `src/` exists yet besides what task 01 produced. This
   task is header-only.

## Notes / risks

- `IButton::HeldMs() const` returning 0 when not down is a contract — make
  sure the doc-comment in the header repeats that, since `MockButton` and
  `DaisyButton` both need to honor it.
- `ILed::SetBrightness` takes a float — clamping happens in implementations,
  not the interface. State this in the header comment.
- `RandomizationProfile.h` may end up with templates if you choose
  `std::array<ParamRange, N>` — that's fine, but keep it header-only and
  forward-compatible with task 06.
