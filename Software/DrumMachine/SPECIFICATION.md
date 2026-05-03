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
- **Outputs**: 4× LED inside the same arcade buttons. Driven by **hardware PWM** — each LED pin must be on a timer-capture/compare channel so brightness updates do not consume CPU. Software PWM is explicitly out of scope. The hardware integrator must select pins backed by four timer channels (any combination of TIM2/3/4/5/8/etc. CCR channels supported by `daisy::Pwm`).
- **Audio**: stereo line-out only (no audio in)
- **Time source**: `daisy::System::GetNow()` for `nowMs` (1 ms resolution)
- **RNG seed source**: `daisy::System::GetUs()` sampled once at boot, before `DrumMachine::Init()`
- **Pin assignments** live in a single header `src/Config.h` so the hardware integrator can change them without touching application code:
  ```cpp
  // src/Config.h
  namespace Config {
      constexpr float    kSampleRate       = 48000.0f;   // Daisy default
      constexpr size_t   kAudioBlockSize   = 48;          // see "Audio Configuration"; implementer may tune
      constexpr uint32_t kHoldThresholdMs  = 500;
      constexpr uint32_t kLedAttackMs      = 5;
      constexpr uint32_t kLedDecayMs       = 120;
      constexpr uint32_t kPulseConfirmOnMs  = 50;          // PulseConfirm shape:
      constexpr uint32_t kPulseConfirmGapMs = 50;          //   on, gap, on, off
      constexpr uint8_t  kPulseConfirmCount = 2;
      constexpr float    kDefaultRandomizationDepth = 0.5f;

      // Hardware pin map — TBD by hardware integrator. Kept here so app code never references pins directly.
      // LED pins MUST be timer-PWM-capable (see Hardware Contract).
      extern const daisy::Pin kButtonPins[4];
      extern const daisy::Pin kLedPins[4];
  }
  ```
  Definitions of `kButtonPins` / `kLedPins` live in `src/Config.cpp`. The hardware integrator owns that file.

## Audio Configuration

- **Sample rate**: 48 kHz
- **Block size**: implementer's choice in the range **1..96 samples** (≤ 2 ms control tick at 48 kHz). Default is 48 (1 ms / 1 kHz control tick). All time-based tests must tolerate any control-tick rate in this range.
- **Channels in**: 0 (audio input is not used)
- **Channels out**: 2 (mono signal duplicated to L+R)
- **Allocation policy**: no heap allocation after `DrumMachine::Init()` returns; no allocation in `Tick()` or `Process()`. (Tests are also no-allocation in the audio path; setup/teardown may allocate freely.)

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
│   │   ├── IInstrument.h       # interface: Init(sampleRate, depth), Trig, Process, Randomize
│   │   ├── BassDrum.{h,cpp}    # wraps daisysp::AnalogBassDrum
│   │   ├── Snare.{h,cpp}       # wraps daisysp::AnalogSnareDrum
│   │   ├── HiHat.{h,cpp}       # wraps daisysp::HiHat<>
│   │   └── Resonator.{h,cpp}   # wraps daisysp::ModalVoice
│   ├── controls/
│   │   ├── IButton.h           # interface
│   │   ├── ILed.h              # interface
│   │   ├── DaisyButton.{h,cpp} # IButton backed by daisy::Switch
│   │   └── DaisyLed.{h,cpp}    # ILed backed by hardware-PWM timer channel
│   ├── randomization/
│   │   ├── IRng.h              # interface
│   │   ├── XorShiftRng.{h,cpp} # default deterministic RNG
│   │   └── RandomizationProfile.h
│   └── util/
│       └── LedTrigger.{h,cpp}  # one-shot envelope + PulseConfirm pattern
└── test/
    ├── support/
    │   ├── test_macros.h       # TEST_CASE / EXPECT_* macros (throw-based; see "Test Harness")
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
    // depth01: 0 = always baseline; 1 = full random range. Init-time only — no runtime setter.
    virtual void  Init(float sampleRate, float depth01) = 0;
    virtual void  Trig() = 0;                // fire the voice with current params
    virtual float Process() = 0;             // one mono sample
    virtual void  Randomize(IRng& rng) = 0;  // re-roll params per RandomizationProfile
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

    // Init is idempotent: every call performs a full state reset.
    //   - forwards (sampleRate, Config::kDefaultRandomizationDepth) to each IInstrument::Init
    //   - clears all LedTrigger envelope state (LEDs go dark)
    //   - re-arms hold detection on every pad
    //   - re-applies the boot-stuck-button mask (see DrumPad Behavior below)
    // Calling Init() a second time is supported and may be used as a "panic reset".
    void  Init(float sampleRate);

    void  Tick(uint32_t nowMs);   // call once per audio block (≤ 2 ms)
    float Process();              // call once per audio sample; returns mono
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

There is no public accessor for individual pads; tests interact with the engine via `TestRig` (see Test Harness), which queries the same `MockLed` / `MockRng` references it injected.

## DrumPad Behavior (the heart of the spec)

`DrumPad` owns one `IButton`, one `ILed`, one `IInstrument`, one `IRng` reference, and one `LedTrigger` envelope.

```
DrumPad::Tick(nowMs):
  button.Update(nowMs)

  // Boot-stuck-button mask: if a button reads "down" on the very first Tick after Init(),
  // suppress its press semantics until it has been observed released at least once.
  // Without this, holding a button at power-on would fire all four drums + a randomization.
  // Implementations must include a brief comment at the masking site explaining this.
  if firstTickAfterInit and button.IsDown():
      pressMaskedUntilRelease = true
  if pressMaskedUntilRelease:
      if button.JustReleased():
          pressMaskedUntilRelease = false
      return  // no Trig, no PulseConfirm, no hold detection while masked

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

**LED envelope shape:** `LedTrigger::Fire(nowMs)` produces a **linear ramp** from `1.0` at `nowMs + Config::kLedAttackMs` down to `0.0` at `nowMs + Config::kLedAttackMs + Config::kLedDecayMs`. Before the attack ends the value rises linearly from 0 to 1. After decay completes the value is exactly 0. Tests assert this shape.

**PulseConfirm vs active envelope:** if `PulseConfirm(nowMs)` is called while a `Fire` envelope is still active, **PulseConfirm wins** — the trigger envelope is canceled and the pulse pattern (`on / gap / on / off`) drives the LED until it completes. This rule is observable: tests can `Fire`, advance 30 ms, then `PulseConfirm`, and assert the next 200 ms matches the pulse pattern, not a sum.

LED behavior: dark when not triggered (matches the "LED lights any time the corresponding drum is triggered" requirement). Toggle state is conveyed only by the `PulseConfirm` blink at the moment of toggle — there is no steady-state indicator.

## Instruments & Randomization

Each instrument wraps a DaisySP voice and owns a `RandomizationProfile`:

```cpp
struct ParamRange { float min; float max; float baseline; };

struct RandomizationProfile {
    // One ParamRange per setter the voice exposes (e.g. freq, decay, tone).
    // depth: 0 → always baseline; 1 → uniform sample in [min, max].
    // Stored on the instrument and set once at Init(sampleRate, depth).
};
```

`Randomize(rng)` for each parameter computes:
```
raw   = lerp(baseline, rng.NextFloat() * (max - min) + min, depth)
value = clamp(raw, min, max)   // tolerates baseline outside [min, max] without UB
```
then calls the corresponding DaisySP setter.

Depth is **init-time only** — passed as the second argument to `IInstrument::Init`. There is no runtime depth control surface (the press-and-hold toggle is a binary on/off). Default depth: `Config::kDefaultRandomizationDepth = 0.5`.

### Voice-by-voice mapping

DaisySP APIs confirmed in `Software/GuitarPedal/dependencies/DaisySP/`; voice setup pattern mirrors `drum_module.cpp:119-129` and `:239-293`. **Decay values are normalized 0..1** as DaisySP's `SetDecay` expects (not seconds). **Accent is fixed at the baseline value** for every voice — it is not in the randomized parameter list, since pad-style players expect consistent loudness.

| Pad       | DaisySP class       | Randomized params                              | Fixed     | Default baseline (musical)               |
|-----------|---------------------|------------------------------------------------|-----------|------------------------------------------|
| Bass      | `AnalogBassDrum`    | freq, decay, tone (`SetSelfFmAmount`)          | accent    | freq=50 Hz, decay=0.6, tone=0.3, accent=0.7 |
| Snare     | `AnalogSnareDrum`   | freq, decay, snappy, tone                      | accent    | freq=200 Hz, decay=0.4, snappy=0.6, tone=0.5, accent=0.7 |
| Hi-hat    | `HiHat<>`           | freq, decay, noisiness, tone                   | accent    | freq=6 kHz, decay=0.2, noisiness=0.7, tone=0.5, accent=0.7 |
| Resonator | `ModalVoice`        | freq, structure, brightness, damping           | accent    | freq=220 Hz, structure=0.4, brightness=0.6, damping=0.5, accent=0.7 |

### Range tuning policy

`ParamRange::min` / `max` for each randomized parameter are chosen by the **implementer** as a best-effort musical first pass — there is no spec-mandated table. The acceptance gate for ranges is the manual hardware audition: with depth = 0.5 each pad must produce sounds that vary audibly press-to-press while staying recognizably in-genre (a kick that always sounds like a kick, a snare that always sounds like a snare). Tuning will be revisited after user testing on the assembled hardware. Tests therefore assert *that* parameters change and stay within `[min, max]`, not that they hit specific values.

Resonator-frequency quantization to a scale is out of scope (see Out of Scope).

## Mixer

`Mixer::Process()` sums the four `instrument.Process()` outputs with per-voice gains. The existing `drum_module.cpp:364-376` uses bass × 6.0, snare × 0.9, hi-hat × 1.0 — these are a *starting reference*, not specification.

**"Peak" definition (used throughout the test harness):**
```
peak(buffer) = max over s in buffer of |buffer[s]|
```
i.e. maximum absolute sample value. RMS is not used.

**Specification (test-driven, not value-driven):**
- Implementer chooses initial gains.
- `test_mixer.cpp` asserts that with all four pads triggered simultaneously at full accent, the captured audio buffer has `peak ≤ 0.95` (no hard clipping).
- `test_instruments.cpp` asserts each voice in isolation has `peak ≥ 0.05` after a `Trig()` (audible).
- Gains are tuned to satisfy both.

Output is mono, written to both stereo channels in `main.cpp`'s `AudioCallback`.

## Test Harness (Native Host Build)

Goal: AI agents can run `make -f Makefile.test` and get a green/red signal that the actual code behaves correctly — including audio.

- **Build target**: `g++ -std=gnu++20 -Wall -Wextra -Werror`. The host build compiles the required DaisySP `.cpp` files **directly from source** as part of `Makefile.test` (DaisySP is portable C++ with no Daisy-hardware dependencies). It does **not** link against the prebuilt arm-none-eabi static library — that lib is firmware-only. Excludes `libDaisy` and the `DaisyButton` / `DaisyLed` / `Config.cpp` / `main.cpp` files (the hardware seam).
- **Test framework**: header-only, hand-rolled in `test/support/test_macros.h`. `EXPECT_*` macros throw a `TestFailure` exception on assertion failure (carrying file/line/message). `TEST_CASE(name)` self-registers into a static registry. `run_all` iterates the registry, wraps each case in `try { … } catch (const TestFailure& e) { record failure } catch (const std::exception& e) { record uncaught } catch (...) { record uncaught }`, and prints `passed/failed/total` accurately even when individual cases throw. Exit code is 0 iff `failed == 0`.
- **Build artifact**: a single binary `build/test/run_all`.
- **TestRig API**:
  ```cpp
  class TestRig {
  public:
      TestRig();                                   // constructs DrumMachine wired to mocks, default seed
      explicit TestRig(uint32_t rngSeed);

      void PressButton(PadIndex pad);              // press at current sim time
      void HoldButton(PadIndex pad, uint32_t ms);  // press, advance ms, release
      void ReleaseButton(PadIndex pad);
      void AdvanceMs(uint32_t ms);                 // advances sim clock; calls Tick() each block; runs no audio
      std::vector<float> CaptureAudio(uint32_t ms); // advances sim clock AND fills buffer at kSampleRate

      float LedBrightness(PadIndex pad) const;
      bool  RandomizationEnabled(PadIndex pad) const;
      uint32_t NowMs() const;
  };
  ```
  `TestRig` is the only test seam into per-pad state — `DrumMachine` itself exposes no public per-pad accessor. `RandomizationEnabled` and `LedBrightness` are read by the `TestRig` from the `MockLed` and from internal bookkeeping it maintains alongside the engine.
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
  - `EXPECT_AUDIO_PEAK_LE(buf, max)` — peak ≤ max (peak = max abs sample)
  - `EXPECT_FIRST_TRANSIENT_WITHIN(buf, sampleIdx)`
  - `EXPECT_LED_FIRED_WITHIN(rig, pad, ms)`
  - `EXPECT_PARAMS_UNCHANGED(instrument)` / `EXPECT_PARAMS_DIFFER(a, b)`
- **Required test cases** (one per file unless noted):
  - `test_drum_pad`: press → `Trig()` fires same tick; `Randomize()` called after `Trig()`; boot-stuck button is masked until released.
  - `test_hold_toggles`: 500 ms hold flips `RandomizationEnabled`; subsequent presses produce identical audio buffers; another 500 ms hold flips it back.
  - `test_led_envelope`: brightness rises linearly 0→1 over `kLedAttackMs`, falls linearly 1→0 over `kLedDecayMs`, is 0 thereafter; `PulseConfirm` shape matches `Config::kPulseConfirm*`; PulseConfirm fired during an active envelope cancels it.
  - `test_instruments`: each voice produces non-silent, bounded output; constructing with depth = 0 freezes params across `Randomize()` calls; depth = 1 spans the configured `[min, max]` range across N samples.
  - `test_randomization`: with `MockRng::SetSequence({...})`, instrument parameters are exactly the expected `clamp(lerp(...))` results.
  - `test_mixer`: all four pads firing at full accent → peak ≤ 0.95.
  - `test_simultaneous_press`: two and four pads pressed in the same control tick all trigger; resulting buffer is non-silent and bounded.
- **Decoupling smoke test** (inside `test_drum_pad`): construct a `DrumPad` with a stub `IInstrument` that records `Trig`/`Randomize` calls; verify the pad drives it correctly without depending on any concrete instrument.

## Critical Files to Create

All new — no existing files are modified:
- `Software/DrumMachine/Makefile`
- `Software/DrumMachine/Makefile.test`
- `Software/DrumMachine/README.md` (see "README Contents" below)
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

### README Contents

`README.md` must contain exactly these four sections (and may not require any others):
1. **Build firmware** — the `make` invocation, prerequisites, and where the resulting `.bin` lands.
2. **Flash firmware** — how to put the Daisy Seed in DFU mode and run `make program-dfu` (or the equivalent), plus the troubleshooting one-liner if `dfu-util` doesn't see the device.
3. **Run host tests** — `make -f Makefile.test && ./build/test/run_all`, expected output shape, and the exit-code contract.
4. **Editing the pin map** — pointer to `src/Config.cpp`, the constraint that LED pins must be timer-PWM-capable, and a note that no other file should reference pins directly.

Total length target: ~30 lines. No marketing copy, no architecture overview (that lives here in `SPECIFICATION.md`).

## Reuse from the Existing Codebase

- **Submodules**: `Software/GuitarPedal/dependencies/libDaisy` and `dependencies/DaisySP` — referenced directly from `Software/DrumMachine/Makefile` via relative path; no duplication.
- **Voice setup pattern** (`Init`, `SetFreq`, `SetDecay`, `Trig`, `Process`): mirror `Software/GuitarPedal/Effect-Modules/drum_module.cpp:119-129` (init) and `:239-293` (trigger).
- **Mixer gain reference**: `Software/GuitarPedal/Effect-Modules/drum_module.cpp:364-376` — starting point only; final values are test-driven (see Mixer section).
- **Build flags / arm-none-eabi setup**: copy from `Software/GuitarPedal/Makefile` (C++20, `-Ofast`, `BOOT_SRAM`).

## Build Configuration

- **Firmware build**: `arm-none-eabi-g++ -std=gnu++20 -Ofast -Wall -Wextra -Werror`. Boot mode `BOOT_SRAM` (matches existing pedal firmware). Links against the prebuilt `libdaisysp.a` and `libdaisy.a` static libraries.
- **Host test build**: `g++ -std=gnu++20 -O2 -g -Wall -Wextra -Werror`. Compiles the required DaisySP `.cpp` files directly from `Software/GuitarPedal/dependencies/DaisySP/Source/` (specifically the four voice classes plus their helpers — the implementer adds files to `Makefile.test`'s source list as needed). DaisySP headers should be included via `-isystem` so DaisySP's own warnings aren't promoted by `-Werror`.
- **Submodule prerequisite (firmware only)**: `libDaisy` and `DaisySP` static libraries must be built before `make`. Reuse the existing helper: `bash Software/GuitarPedal/ci/build_libs.sh` (skips CloudSeed/RTNeural — those aren't needed here). The host test build does **not** require this step. Document this in `README.md`.

## Verification

End-to-end checks the implementer (or an AI agent) runs after building:

1. **Host tests**: `cd Software/DrumMachine && make -f Makefile.test && ./build/test/run_all` → exit code 0; output reports all test cases passed (`passed/failed/total` line with `failed == 0`).
2. **Firmware build**: `cd Software/DrumMachine && make` → produces a `.bin` for the Daisy Seed with no warnings (`-Werror` enforces this).
3. **Manual hardware smoke test** (out of scope for AI agents, listed for completeness):
   - Press each button → corresponding LED flashes and drum sound plays.
   - Press the same button rapidly → sound varies on each press (randomization is working) and stays in-genre.
   - Hold any button >500 ms → LED double-blinks confirm pattern; subsequent presses produce identical sounds (randomization disabled). Hold again → variation returns.
   - Press all four buttons simultaneously → all four sounds play, no audible clipping.
   - Power on with a finger held on any button → that drum does **not** fire on boot; releasing and pressing it triggers normally.

## Out of Scope (Dream List, not implemented now)

- Multi-button combo modes (synth/looper)
- Persistent storage of "locked" sounds across power cycles
- Pitch quantization for the resonator
- MIDI in/out
- Runtime control of randomization depth (currently init-time only)
- Per-voice accent randomization (intentionally fixed)

The interfaces above (`IInstrument`, `IButton`, `ILed`, `IRng`) are the seams these features would plug into later.
