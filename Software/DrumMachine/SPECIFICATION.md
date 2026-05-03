# Four-Button Drum Machine — Specification

## Context

Build a new standalone firmware project for a four-button drum machine on the Daisy Seed. Each button is an Adafruit arcade button with an integrated LED, mapped to one drum voice: **bass drum**, **snare**, **hi-hat**, **resonator**. There are no other controls — power and volume are external hardware.

Behavior per button:
- **Press**: trigger the voice immediately, light the LED, then re-randomize that voice's parameters for the next trigger.
- **Press-and-hold** (≥500 ms): toggle randomization on/off for that pad. The press itself still triggers the sound.

The architecture must be cleanly decoupled so instruments, button behavior, and (eventually) modes can be swapped. A native-host test harness lets AI agents verify behavior without flashing hardware.

This project lives alongside the existing `Software/GuitarPedal/` firmware. It reuses the `libDaisy` and `DaisySP` git submodules already wired into the repo, but does **not** depend on the guitar pedal codebase.

## Project Layout

```
Software/DrumMachine/
├── Makefile                    # firmware target (arm-none-eabi)
├── Makefile.test               # host target (g++/clang++) for tests
├── README.md                   # build & flash instructions
├── src/
│   ├── main.cpp                # firmware entry: wires HW → DrumMachine, runs audio cb
│   ├── DrumMachine.{h,cpp}     # top-level engine: owns pads, mixer, audio loop
│   ├── DrumPad.{h,cpp}         # ties one IButton + ILed + IInstrument together
│   ├── Mixer.{h,cpp}           # per-voice gain, sum to stereo out
│   ├── instruments/
│   │   ├── IInstrument.h       # interface: Init, Trig, Process, Randomize
│   │   ├── BassDrum.{h,cpp}    # wraps daisysp::AnalogBassDrum
│   │   ├── Snare.{h,cpp}       # wraps daisysp::AnalogSnareDrum
│   │   ├── HiHat.{h,cpp}       # wraps daisysp::HiHat<>
│   │   └── Resonator.{h,cpp}   # wraps daisysp::ModalVoice
│   ├── controls/
│   │   ├── IButton.h           # interface: Update, IsDown, JustPressed, JustReleased, HeldMs
│   │   ├── ILed.h              # interface: SetBrightness(0..1)
│   │   ├── DaisyButton.{h,cpp} # IButton backed by daisy::Switch
│   │   └── DaisyLed.{h,cpp}    # ILed backed by GPIO/PWM
│   ├── randomization/
│   │   ├── IRng.h              # interface: NextFloat() ∈ [0,1)
│   │   ├── XorShiftRng.{h,cpp} # default deterministic RNG
│   │   └── RandomizationProfile.h  # per-param {min, max} + depth (0..1) per voice
│   └── util/
│       └── LedTrigger.{h,cpp}  # one-shot LED envelope (attack/decay) on Trig()
└── test/
    ├── Makefile.test           # included from top-level
    ├── support/
    │   ├── MockButton.{h,cpp}  # scriptable IButton for tests
    │   ├── MockLed.{h,cpp}     # records SetBrightness calls
    │   ├── MockRng.{h,cpp}     # deterministic, scriptable
    │   └── TestRig.{h,cpp}     # owns DrumMachine + mocks; press(idx, ms), tick(ms), capture()
    ├── test_drum_pad.cpp       # button → trigger → randomize sequence
    ├── test_hold_toggles.cpp   # 500 ms hold flips randomizer enable
    ├── test_led_envelope.cpp   # LED on at trigger, fades within window
    ├── test_instruments.cpp    # each voice: Trig produces non-silent buffer; Process is bounded
    ├── test_randomization.cpp  # depth=0 → params unchanged; depth=1 → params span configured range
    └── test_mixer.cpp          # voices sum without clipping at default gains
```

## Core Interfaces

```cpp
// src/instruments/IInstrument.h
class IInstrument {
public:
    virtual ~IInstrument() = default;
    virtual void Init(float sampleRate) = 0;
    virtual void Trig() = 0;             // fire the voice with current params
    virtual float Process() = 0;         // one mono sample
    virtual void Randomize(IRng& rng) = 0;
    virtual void SetRandomizationDepth(float depth01) = 0;  // 0 = freeze, 1 = full range
};

// src/controls/IButton.h
class IButton {
public:
    virtual ~IButton() = default;
    virtual void Update(uint32_t nowMs) = 0;
    virtual bool IsDown() const = 0;
    virtual bool JustPressed() const = 0;     // edge: rising this Update()
    virtual bool JustReleased() const = 0;    // edge: falling this Update()
    virtual uint32_t HeldMs() const = 0;      // 0 if not currently down
};

// src/controls/ILed.h
class ILed {
public:
    virtual ~ILed() = default;
    virtual void SetBrightness(float v01) = 0;  // 0..1, clamped
};
```

## DrumPad Behavior (the heart of the spec)

`DrumPad` owns one `IButton`, one `ILed`, one `IInstrument`, and one `LedTrigger` envelope.

```
On each control tick (called from audio block boundary, ~1 kHz):
  button.Update(nowMs)

  if button.JustPressed():
      instrument.Trig()              // requirement: trigger immediately
      ledTrigger.Fire(nowMs)
      if randomizationEnabled:
          instrument.Randomize(rng)  // requirement: randomize after trigger
      holdToggleArmed = true

  if button.IsDown() and holdToggleArmed and button.HeldMs() >= HOLD_THRESHOLD_MS:
      randomizationEnabled = !randomizationEnabled
      ledTrigger.PulseConfirm(nowMs) // short double-blink to confirm toggle
      holdToggleArmed = false        // don't re-toggle until next press

  if button.JustReleased():
      holdToggleArmed = false

  led.SetBrightness(ledTrigger.Brightness(nowMs))
```

Constants (defined in `DrumPad.h`, easy to tune):
- `HOLD_THRESHOLD_MS = 500`
- `LED_ATTACK_MS = 5`, `LED_DECAY_MS = 120` (one-shot envelope on trigger)
- `LED_IDLE_GLOW = 0.0f`, `LED_DISABLED_GLOW = 0.0f` (LED is dark when not triggered; toggle state is conveyed via the confirm-blink, not steady-state — keeps spec aligned with "LED lights any time the corresponding drum is triggered")

## Instruments & Randomization

Each instrument wraps a DaisySP voice and owns a `RandomizationProfile`:

```cpp
struct ParamRange { float min; float max; float baseline; };
struct RandomizationProfile {
    // one ParamRange per setter the voice exposes (e.g. freq, decay, tone…)
    // depth: 0 → always baseline; 1 → uniform sample in [min, max]
};
```

`Randomize(rng)` for each parameter computes:
`value = lerp(baseline, rng.NextFloat() * (max - min) + min, depth)`

then calls the corresponding DaisySP setter.

Voice-by-voice mapping (DaisySP APIs confirmed in `dependencies/DaisySP/`):

| Pad       | DaisySP class       | Randomized params                                       | Default baseline (musical) |
|-----------|---------------------|---------------------------------------------------------|----------------------------|
| Bass      | `AnalogBassDrum`    | freq, decay, tone (SetSelfFmAmount), accent             | 50 Hz, 0.6s, 0.3, 0.6      |
| Snare     | `AnalogSnareDrum`   | freq, decay, snappy, tone, accent                       | 200 Hz, 0.3s, 0.6, 0.5, 0.7 |
| Hi-hat    | `HiHat<>`           | freq, decay, noisiness, tone, accent                    | 6 kHz, 0.15s, 0.7, 0.5, 0.7 |
| Resonator | `ModalVoice`        | freq (within scale), structure, brightness, damping, accent | 220 Hz, 0.4, 0.6, 0.5, 0.7 |

`SetRandomizationDepth(d)` is per-instrument so each voice can be tuned independently. Default depth: **0.5** for all.

The resonator's freq range can later be quantized to a scale (out of scope; baseline is free-running for now).

## Mixer

`Mixer::Process()` sums the four `instrument.Process()` outputs with per-voice gains (mirroring the existing `drum_module.cpp` scaling that already sounds balanced):
- bass × 6.0, snare × 0.9, hi-hat × 1.0, resonator × 1.0

Output is mono, written to both stereo channels in `main.cpp`'s `AudioCallback`.

## Test Harness (Native Host Build)

Goal: AI agents can run `make -f Makefile.test` and get a green/red signal that the actual code behaves correctly — including audio.

- **Build target**: `g++ -std=gnu++20`, links against the same DaisySP source as firmware (DaisySP is portable C++ with no Daisy-hardware dependencies). Excludes `libDaisy` and the `DaisyButton`/`DaisyLed` files.
- **Test framework**: header-only, hand-rolled (`assert`-based macros in `test/support/test_macros.h`) — no extra submodules. Each `test_*.cpp` builds to a binary; `Makefile.test` runs them in sequence and reports pass/fail counts.
- **TestRig API** (the surface AI agents script against):
  ```cpp
  rig.PressButton(PadIndex pad);
  rig.HoldButton(PadIndex pad, uint32_t durationMs);
  rig.ReleaseButton(PadIndex pad);
  rig.AdvanceMs(uint32_t ms);                  // ticks control + audio
  std::vector<float> rig.CaptureAudio(uint32_t ms);
  float rig.LedBrightness(PadIndex pad) const;
  bool  rig.RandomizationEnabled(PadIndex pad) const;
  ```
- **Assertion helpers**: `ExpectAudioNotSilent(buf)`, `ExpectAudioBoundedBy(buf, peak)`, `ExpectFirstTransientWithin(buf, sampleIndex)`, `ExpectLedFiredWithin(rig, pad, ms)`.
- **Determinism**: tests inject `MockRng` with a fixed seed; same input always yields identical audio buffers.

## Critical Files to Create

All new — no existing files are modified:
- `Software/DrumMachine/Makefile`
- `Software/DrumMachine/Makefile.test`
- `Software/DrumMachine/src/main.cpp`
- `Software/DrumMachine/src/DrumMachine.{h,cpp}`
- `Software/DrumMachine/src/DrumPad.{h,cpp}`
- `Software/DrumMachine/src/Mixer.{h,cpp}`
- `Software/DrumMachine/src/instruments/IInstrument.h`
- `Software/DrumMachine/src/instruments/{BassDrum,Snare,HiHat,Resonator}.{h,cpp}`
- `Software/DrumMachine/src/controls/{IButton.h, ILed.h, DaisyButton.{h,cpp}, DaisyLed.{h,cpp}}`
- `Software/DrumMachine/src/randomization/{IRng.h, XorShiftRng.{h,cpp}, RandomizationProfile.h}`
- `Software/DrumMachine/src/util/LedTrigger.{h,cpp}`
- `Software/DrumMachine/test/support/{MockButton, MockLed, MockRng, TestRig}.{h,cpp}` + `test_macros.h`
- `Software/DrumMachine/test/test_*.cpp` (six files listed in layout)

## Reuse from the Existing Codebase

- **Submodules**: `Software/GuitarPedal/dependencies/libDaisy` and `dependencies/DaisySP` — referenced directly from `Software/DrumMachine/Makefile` via relative path; no duplication.
- **Voice setup conventions** (Trig + setter pattern, gain scaling): mirror `Software/GuitarPedal/Effect-Modules/drum_module.cpp` so the sound character is in a known-good ballpark.
- **Build flags / arm-none-eabi setup**: copy from `Software/GuitarPedal/Makefile` (C++20, `-Ofast`, BOOT_SRAM).

## Verification

End-to-end checks the implementer (or an AI agent) runs after building:

1. **Host tests**: `cd Software/DrumMachine && make -f Makefile.test && ./build/test/run_all` → all six test binaries pass.
2. **Firmware build**: `cd Software/DrumMachine && make` → produces a `.bin` for the Daisy Seed without warnings.
3. **Manual hardware smoke test** (out of scope for AI agents, listed for completeness):
   - Press each button → corresponding LED flashes and drum sound plays.
   - Press the same button rapidly → sound varies on each press (randomization is working).
   - Hold any button >500 ms → LED blinks confirm pattern; subsequent presses produce identical sounds (randomization disabled). Hold again → variation returns.
4. **Decoupling smoke test** (host): swap `BassDrum` for a stub `IInstrument` in a test, confirm `DrumPad` still triggers/randomizes correctly — proves the interface boundary holds.

## Out of Scope (Dream List, not implemented now)

- Multi-button combo modes (synth/looper)
- Persistent storage of "locked" sounds across power cycles
- Pitch quantization for the resonator
- MIDI in/out

The interfaces above (`IInstrument`, `IButton`) are the seams these features would plug into later.
