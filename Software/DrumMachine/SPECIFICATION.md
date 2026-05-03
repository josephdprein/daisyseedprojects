# Four-Button Drum Machine — Specification

## Context

Build a new standalone firmware project for a four-button drum machine on the Daisy Seed. Each button is an Adafruit arcade button with an integrated LED, mapped to one drum voice: **bass drum**, **snare**, **hi-hat**, **resonator**. There are no other controls — power and volume are external hardware.

Behavior per button:
- **Press**: trigger the voice immediately, light the LED, then re-randomize that voice's parameters for the next trigger.
- **Press-and-hold** (≥500 ms): toggle randomization on/off for that pad. The press itself still triggers the sound.

The architecture must be cleanly decoupled so instruments, button behavior, and (eventually) modes can be swapped. A native-host test harness lets AI agents verify behavior without flashing hardware.

This project lives alongside the existing `Software/GuitarPedal/` firmware. It reuses the `libDaisy` and `DaisySP` git submodules already wired into the repo, but does **not** depend on the guitar pedal codebase.

## Hardware Contract

The implementer is **not** responsible for the physical hardware design, but the firmware must conform to the following contract:

- **MCU**: Daisy Seed (STM32H750)
- **Inputs**: 4× Adafruit arcade button (active-low, internal pull-up)
- **Outputs**: 4× LED inside the same arcade buttons (PWM-capable GPIO; brightness 0..1)
- **Audio**: stereo line-out only (no audio in)
- **Time source**: `daisy::System::GetNow()` for `nowMs` (1 ms resolution)
- **RNG seed source**: `daisy::System::GetUs()` sampled once at boot, before `DrumMachine::Init()`
- **Pin assignments** live in a single header `src/Config.h` so the hardware integrator can change them without touching application code:
  ```cpp
  // src/Config.h
  namespace Config {
      constexpr float    kSampleRate       = 48000.0f;   // Daisy default
      constexpr size_t   kAudioBlockSize   = 48;          // 1 ms @ 48 kHz → control tick rate = 1 kHz
      constexpr uint32_t kHoldThresholdMs  = 500;
      constexpr uint32_t kLedAttackMs      = 5;
      constexpr uint32_t kLedDecayMs       = 120;
      constexpr uint32_t kPulseConfirmOnMs  = 50;          // PulseConfirm shape:
      constexpr uint32_t kPulseConfirmGapMs = 50;          //   on, gap, on, off
      constexpr uint8_t  kPulseConfirmCount = 2;
      constexpr float    kDefaultRandomizationDepth = 0.5f;

      // Hardware pin map — TBD by hardware integrator. Kept here so app code never references pins directly.
      extern const daisy::Pin kButtonPins[4];
      extern const daisy::Pin kLedPins[4];
  }
  ```
  Definitions of `kButtonPins` / `kLedPins` live in `src/Config.cpp`. The hardware integrator owns that file.

## Audio Configuration

- **Sample rate**: 48 kHz
- **Block size**: 48 samples (= 1 ms = 1 kHz control tick rate)
- **Channels in**: 0 (audio input is not used)
- **Channels out**: 2 (mono signal duplicated to L+R)
- **Allocation policy**: no heap allocation after `DrumMachine::Init()` returns; no allocation in `Tick()` or `Process()`.

## Project Layout

```
Software/DrumMachine/
├── Makefile                    # firmware target (arm-none-eabi)
├── Makefile.test               # host target (g++/clang++) for tests
├── README.md                   # build & flash instructions
├── src/
│   ├── main.cpp                # firmware entry: wires HW → DrumMachine, runs audio cb
│   ├── Config.h                # constants + pin map declarations (see Hardware Contract)
│   ├── Config.cpp              # pin map definitions (hardware integrator owns this)
│   ├── PadIndex.h              # enum class PadIndex
│   ├── DrumMachine.{h,cpp}     # top-level engine: owns pads, mixer
│   ├── DrumPad.{h,cpp}         # ties one IButton + ILed + IInstrument together
│   ├── Mixer.{h,cpp}           # per-voice gain, sum to mono
│   ├── instruments/
│   │   ├── IInstrument.h       # interface: Init, Trig, Process, Randomize, SetRandomizationDepth
│   │   ├── BassDrum.{h,cpp}    # wraps daisysp::AnalogBassDrum
│   │   ├── Snare.{h,cpp}       # wraps daisysp::AnalogSnareDrum
│   │   ├── HiHat.{h,cpp}       # wraps daisysp::HiHat<>
│   │   └── Resonator.{h,cpp}   # wraps daisysp::ModalVoice
│   ├── controls/
│   │   ├── IButton.h           # interface
│   │   ├── ILed.h              # interface
│   │   ├── DaisyButton.{h,cpp} # IButton backed by daisy::Switch
│   │   └── DaisyLed.{h,cpp}    # ILed backed by GPIO/PWM
│   ├── randomization/
│   │   ├── IRng.h              # interface
│   │   ├── XorShiftRng.{h,cpp} # default deterministic RNG
│   │   └── RandomizationProfile.h
│   └── util/
│       └── LedTrigger.{h,cpp}  # one-shot envelope + PulseConfirm pattern
└── test/
    ├── support/
    │   ├── test_macros.h       # TEST_CASE / EXPECT_* macros (assert-based)
    │   ├── MockButton.{h,cpp}
    │   ├── MockLed.{h,cpp}
    │   ├── MockRng.{h,cpp}
    │   └── TestRig.{h,cpp}
    ├── test_drum_pad.cpp
    ├── test_hold_toggles.cpp
    ├── test_led_envelope.cpp
    ├── test_instruments.cpp
    ├── test_randomization.cpp
    ├── test_mixer.cpp
    ├── test_simultaneous_press.cpp
    └── main_test.cpp           # links all test_*.cpp into a single binary `run_all`
```

## Type Definitions

```cpp
// src/PadIndex.h
enum class PadIndex : uint8_t {
    Bass      = 0,
    Snare     = 1,
    HiHat     = 2,
    Resonator = 3,
    Count     = 4
};
```

## Core Interfaces

```cpp
// src/instruments/IInstrument.h
class IInstrument {
public:
    virtual ~IInstrument() = default;
    virtual void  Init(float sampleRate) = 0;
    virtual void  Trig() = 0;                // fire the voice with current params
    virtual float Process() = 0;             // one mono sample
    virtual void  Randomize(IRng& rng) = 0;  // re-roll params per RandomizationProfile
    virtual void  SetRandomizationDepth(float depth01) = 0;  // init-time only; 0 = freeze, 1 = full range
};

// src/controls/IButton.h
class IButton {
public:
    virtual ~IButton() = default;
    virtual void     Update(uint32_t nowMs) = 0;
    virtual bool     IsDown() const = 0;
    virtual bool     JustPressed() const = 0;   // edge: rising this Update()
    virtual bool     JustReleased() const = 0;  // edge: falling this Update()
    virtual uint32_t HeldMs() const = 0;        // 0 if not currently down
};

// src/controls/ILed.h
class ILed {
public:
    virtual ~ILed() = default;
    virtual void SetBrightness(float v01) = 0;  // 0..1, implementations must clamp
};

// src/randomization/IRng.h
class IRng {
public:
    virtual ~IRng() = default;
    virtual float NextFloat() = 0;  // uniform in [0, 1)
};
```

## Ownership Model

- `main.cpp` owns (as stack/static objects, no heap) the 4 `DaisyButton`s, 4 `DaisyLed`s, 4 instrument instances, 1 `XorShiftRng`, and 1 `DrumMachine`.
- `DrumMachine` borrows everything by reference; lifetime is the caller's problem.
- `DrumPad` borrows its `IButton`, `ILed`, `IInstrument`, and `IRng` by reference, captured at construction.
- The same model applies in tests (the `TestRig` substitutes `Mock*` objects for the `Daisy*` ones).

Rationale: no `std::unique_ptr` / `new` in audio firmware; explicit lifetimes via stack ownership are easier to reason about and impossible to leak.

## DrumMachine API

```cpp
// src/DrumMachine.h
class DrumMachine {
public:
    DrumMachine(
        std::array<IButton*, static_cast<size_t>(PadIndex::Count)>     buttons,
        std::array<ILed*,    static_cast<size_t>(PadIndex::Count)>     leds,
        std::array<IInstrument*, static_cast<size_t>(PadIndex::Count)> instruments,
        IRng& rng);

    void  Init(float sampleRate);     // forwards to instruments + LedTriggers
    void  Tick(uint32_t nowMs);       // call once per audio block (~1 kHz)
    float Process();                  // call once per audio sample; returns mono
    DrumPad& Pad(PadIndex i);
};
```

The audio callback in `main.cpp` looks like:

```cpp
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    machine.Tick(daisy::System::GetNow());          // control rate: once per block
    for (size_t s = 0; s < size; ++s) {
        const float y = machine.Process();          // audio rate
        out[0][s] = y;
        out[1][s] = y;
    }
}
```

## DrumPad Behavior (the heart of the spec)

`DrumPad` owns one `IButton`, one `ILed`, one `IInstrument`, one `IRng` reference, and one `LedTrigger` envelope.

```
DrumPad::Tick(nowMs):
  button.Update(nowMs)

  if button.JustPressed():
      instrument.Trig()              // requirement: trigger immediately
      ledTrigger.Fire(nowMs)
      if randomizationEnabled:
          instrument.Randomize(rng)  // requirement: randomize after trigger
      holdToggleArmed = true

  if button.IsDown() and holdToggleArmed and button.HeldMs() >= Config::kHoldThresholdMs:
      randomizationEnabled = !randomizationEnabled
      ledTrigger.PulseConfirm(nowMs) // see Config::kPulseConfirm* shape
      holdToggleArmed = false        // don't re-toggle until next press

  if button.JustReleased():
      holdToggleArmed = false

  led.SetBrightness(ledTrigger.Brightness(nowMs))
```

`DrumMachine::Tick()` calls each pad's `Tick()` in turn; multiple pads pressed in the same control tick all trigger in that tick, and `Process()` sums their outputs.

**First-press semantics (important for testers):** the very first `Trig()` after `Init()` uses the baseline parameter values. Each `Randomize()` then populates the parameters used by the *next* `Trig()`. Disabling randomization freezes whatever values were last loaded — including any random values from a previous press.

LED behavior: dark when not triggered (matches the "LED lights any time the corresponding drum is triggered" requirement). Toggle state is conveyed only by the `PulseConfirm` blink at the moment of toggle — there is no steady-state indicator.

## Instruments & Randomization

Each instrument wraps a DaisySP voice and owns a `RandomizationProfile`:

```cpp
struct ParamRange { float min; float max; float baseline; };

struct RandomizationProfile {
    // One ParamRange per setter the voice exposes (e.g. freq, decay, tone).
    // depth: 0 → always baseline; 1 → uniform sample in [min, max].
    // Stored on the instrument (not the profile) and set once at Init().
};
```

`Randomize(rng)` for each parameter computes:
`value = lerp(baseline, rng.NextFloat() * (max - min) + min, depth)`
then calls the corresponding DaisySP setter.

`SetRandomizationDepth` is **init-time only**. There is no runtime control surface for depth (the press-and-hold toggle is a binary on/off). Default depth: `Config::kDefaultRandomizationDepth = 0.5`.

Voice-by-voice mapping (DaisySP APIs confirmed in `Software/GuitarPedal/dependencies/DaisySP/`; voice setup pattern mirrors `drum_module.cpp:119-129` and `:239-293`):

| Pad       | DaisySP class       | Randomized params                                       | Default baseline (musical) |
|-----------|---------------------|---------------------------------------------------------|----------------------------|
| Bass      | `AnalogBassDrum`    | freq, decay, tone (`SetSelfFmAmount`), accent           | 50 Hz, 0.6 s, 0.3, 0.6     |
| Snare     | `AnalogSnareDrum`   | freq, decay, snappy, tone, accent                       | 200 Hz, 0.3 s, 0.6, 0.5, 0.7 |
| Hi-hat    | `HiHat<>`           | freq, decay, noisiness, tone, accent                    | 6 kHz, 0.15 s, 0.7, 0.5, 0.7 |
| Resonator | `ModalVoice`        | freq, structure, brightness, damping, accent            | 220 Hz, 0.4, 0.6, 0.5, 0.7 |

Resonator-frequency quantization to a scale is out of scope (see Out of Scope).

## Mixer

`Mixer::Process()` sums the four `instrument.Process()` outputs with per-voice gains. The existing `drum_module.cpp:364-376` uses bass × 6.0, snare × 0.9, hi-hat × 1.0 — these are a *starting reference*, not specification.

**Specification (test-driven, not value-driven):**
- Implementer chooses initial gains.
- `test_mixer.cpp` asserts that with all four pads triggered simultaneously at maximum accent, the resulting audio buffer has `peak ≤ 0.95` (no hard clipping).
- `test_instruments.cpp` asserts each voice in isolation has `peak ≥ 0.05` after a `Trig()` (audible).
- Gains are tuned to satisfy both.

Output is mono, written to both stereo channels in `main.cpp`'s `AudioCallback`.

## Test Harness (Native Host Build)

Goal: AI agents can run `make -f Makefile.test` and get a green/red signal that the actual code behaves correctly — including audio.

- **Build target**: `g++ -std=gnu++20 -Wall -Wextra -Werror`, links against the same DaisySP source as firmware (DaisySP is portable C++ with no Daisy-hardware dependencies). Excludes `libDaisy` and the `DaisyButton` / `DaisyLed` / `Config.cpp` / `main.cpp` files (the hardware seam).
- **Test framework**: header-only, hand-rolled (`assert`-based macros in `test/support/test_macros.h`) — no extra submodules.
- **Build artifact**: a single binary `build/test/run_all` (linked from `main_test.cpp` + all `test_*.cpp` + the `support/` files + the host-portable subset of `src/`). Each `TEST_CASE(name)` self-registers; `run_all` iterates them and prints `passed/failed/total`. Exit code is 0 iff all pass.
- **TestRig API**:
  ```cpp
  class TestRig {
  public:
      TestRig();                                   // constructs DrumMachine wired to mocks, default seed
      explicit TestRig(uint32_t rngSeed);

      void PressButton(PadIndex pad);              // press at current sim time
      void HoldButton(PadIndex pad, uint32_t ms);  // press, advance ms, release
      void ReleaseButton(PadIndex pad);
      void AdvanceMs(uint32_t ms);                 // advances sim clock; calls Tick() each ms; runs no audio
      std::vector<float> CaptureAudio(uint32_t ms); // advances sim clock AND fills buffer at kSampleRate

      float LedBrightness(PadIndex pad) const;
      bool  RandomizationEnabled(PadIndex pad) const;
      uint32_t NowMs() const;
  };
  ```
- **Mock APIs**:
  ```cpp
  class MockButton : public IButton {
  public:
      void ScriptPress(uint32_t atMs);             // multiple events allowed; queued
      void ScriptRelease(uint32_t atMs);
      // IButton methods replay the script as Update(nowMs) advances.
  };
  class MockLed : public ILed {
  public:
      const std::vector<std::pair<uint32_t, float>>& BrightnessHistory() const; // (nowMs, value) on each SetBrightness
      float Last() const;
  };
  class MockRng : public IRng {
  public:
      explicit MockRng(uint32_t seed);             // deterministic xorshift
      void SetSequence(std::vector<float> values); // overrides; next NextFloat() pops front
  };
  ```
- **Assertion helpers**:
  - `EXPECT_AUDIO_NOT_SILENT(buf)` — peak ≥ 0.05
  - `EXPECT_AUDIO_PEAK_LE(buf, max)`
  - `EXPECT_FIRST_TRANSIENT_WITHIN(buf, sampleIdx)`
  - `EXPECT_LED_FIRED_WITHIN(rig, pad, ms)`
  - `EXPECT_PARAMS_UNCHANGED(instrument)` / `EXPECT_PARAMS_DIFFER(a, b)`
- **Required test cases** (one per file unless noted):
  - `test_drum_pad`: press → `Trig()` fires same tick; `Randomize()` called after `Trig()`.
  - `test_hold_toggles`: 500 ms hold flips `RandomizationEnabled`; subsequent presses produce identical audio buffers; another 500 ms hold flips it back.
  - `test_led_envelope`: LED brightness > 0 within 5 ms of trigger; ≤ 0.05 by 130 ms; `PulseConfirm` shape matches `Config::kPulseConfirm*`.
  - `test_instruments`: each voice produces non-silent, bounded output; depth = 0 freezes params across `Randomize()` calls; depth = 1 spans the configured range across N samples.
  - `test_randomization`: with `MockRng::SetSequence({...})`, instrument parameters are exactly the expected lerp results.
  - `test_mixer`: all four pads firing at max accent → peak ≤ 0.95.
  - `test_simultaneous_press`: two and four pads pressed in the same control tick all trigger; resulting buffer is non-silent and bounded.
- **Decoupling smoke test** (inside `test_drum_pad`): construct a `DrumPad` with a stub `IInstrument` that records `Trig`/`Randomize` calls; verify the pad drives it correctly without depending on any concrete instrument.

## Critical Files to Create

All new — no existing files are modified:
- `Software/DrumMachine/Makefile`
- `Software/DrumMachine/Makefile.test`
- `Software/DrumMachine/README.md`
- `Software/DrumMachine/src/main.cpp`
- `Software/DrumMachine/src/Config.{h,cpp}`
- `Software/DrumMachine/src/PadIndex.h`
- `Software/DrumMachine/src/DrumMachine.{h,cpp}`
- `Software/DrumMachine/src/DrumPad.{h,cpp}`
- `Software/DrumMachine/src/Mixer.{h,cpp}`
- `Software/DrumMachine/src/instruments/IInstrument.h`
- `Software/DrumMachine/src/instruments/{BassDrum,Snare,HiHat,Resonator}.{h,cpp}`
- `Software/DrumMachine/src/controls/{IButton.h, ILed.h, DaisyButton.{h,cpp}, DaisyLed.{h,cpp}}`
- `Software/DrumMachine/src/randomization/{IRng.h, XorShiftRng.{h,cpp}, RandomizationProfile.h}`
- `Software/DrumMachine/src/util/LedTrigger.{h,cpp}`
- `Software/DrumMachine/test/support/{test_macros.h, MockButton.{h,cpp}, MockLed.{h,cpp}, MockRng.{h,cpp}, TestRig.{h,cpp}}`
- `Software/DrumMachine/test/main_test.cpp`
- `Software/DrumMachine/test/test_*.cpp` (seven files listed in layout)

## Reuse from the Existing Codebase

- **Submodules**: `Software/GuitarPedal/dependencies/libDaisy` and `dependencies/DaisySP` — referenced directly from `Software/DrumMachine/Makefile` via relative path; no duplication.
- **Voice setup pattern** (`Init`, `SetFreq`, `SetDecay`, `Trig`, `Process`): mirror `Software/GuitarPedal/Effect-Modules/drum_module.cpp:119-129` (init) and `:239-293` (trigger).
- **Mixer gain reference**: `Software/GuitarPedal/Effect-Modules/drum_module.cpp:364-376` — starting point only; final values are test-driven (see Mixer section).
- **Build flags / arm-none-eabi setup**: copy from `Software/GuitarPedal/Makefile` (C++20, `-Ofast`, `BOOT_SRAM`).

## Build Configuration

- **Firmware build**: `arm-none-eabi-g++ -std=gnu++20 -Ofast -Wall -Wextra -Werror`. Boot mode `BOOT_SRAM` (matches existing pedal firmware).
- **Host test build**: `g++ -std=gnu++20 -O2 -g -Wall -Wextra -Werror`.
- **Submodule prerequisite**: `libDaisy` and `DaisySP` static libraries must be built before `make`. Reuse the existing helper: `bash Software/GuitarPedal/ci/build_libs.sh` (skips CloudSeed/RTNeural — those aren't needed here). Document this in `README.md`.

## Verification

End-to-end checks the implementer (or an AI agent) runs after building:

1. **Host tests**: `cd Software/DrumMachine && make -f Makefile.test && ./build/test/run_all` → exit code 0; output reports all test cases passed.
2. **Firmware build**: `cd Software/DrumMachine && make` → produces a `.bin` for the Daisy Seed with no warnings (`-Werror` enforces this).
3. **Manual hardware smoke test** (out of scope for AI agents, listed for completeness):
   - Press each button → corresponding LED flashes and drum sound plays.
   - Press the same button rapidly → sound varies on each press (randomization is working).
   - Hold any button >500 ms → LED double-blinks confirm pattern; subsequent presses produce identical sounds (randomization disabled). Hold again → variation returns.
   - Press all four buttons simultaneously → all four sounds play, no audible clipping.

## Out of Scope (Dream List, not implemented now)

- Multi-button combo modes (synth/looper)
- Persistent storage of "locked" sounds across power cycles
- Pitch quantization for the resonator
- MIDI in/out
- Runtime control of randomization depth (currently init-time only)

The interfaces above (`IInstrument`, `IButton`, `ILed`, `IRng`) are the seams these features would plug into later.
