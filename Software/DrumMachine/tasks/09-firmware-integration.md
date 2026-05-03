# 09 — Firmware integration

## Goal

Land the hardware-facing seam (`DaisyButton`, `DaisyLed`, `Config.cpp`,
`main.cpp`) and the firmware `Makefile`. Produce `build/DrumMachine.bin`
on `arm-none-eabi-g++`. Polish `README.md`.

## Dependencies

- Task 08 (DrumMachine engine).
- Built libDaisy and DaisySP static libs:
  `bash Software/GuitarPedal/ci/build_libs.sh`.

## Files to create

- `src/controls/DaisyButton.{h,cpp}` — implements `IButton` over `daisy::Switch`.
- `src/controls/DaisyLed.{h,cpp}` — implements `ILed` over hardware PWM.
- `src/Config.cpp` — pin-map definitions for `kButtonPins[4]` / `kLedPins[4]`.
- `src/main.cpp` — entry point, audio callback, main loop.
- `Software/DrumMachine/Makefile` — firmware build target.
- Update `Software/DrumMachine/README.md` to match spec §README Contents.

## DaisyButton

- Wraps a `daisy::Switch` initialized with the `daisy::Pin` from `Config::kButtonPins`.
- `Update(nowMs)`: calls `Switch::Debounce()`. The Daisy `Switch` already
  exposes `Pressed()`, `RisingEdge()`, `FallingEdge()`, `TimeHeldMs()` —
  thin-wrap them for `IsDown`, `JustPressed`, `JustReleased`, `HeldMs`.
- Match the active-low + internal pull-up contract per spec
  §Hardware Contract: `Switch::Init(pin, sampleRate, Switch::TYPE_MOMENTARY,
  Switch::POLARITY_INVERTED, Switch::PULL_UP)` (verify exact enum names
  against libDaisy).
- `JustPressed/JustReleased` are edges *during the current Update*, not
  standing flags — match `MockButton`'s contract.

## DaisyLed

- Wraps a hardware PWM channel for the LED pin. Per spec, PWM **must be
  hardware-driven** — software PWM is out of scope.
- **Preflight check:** verify libDaisy's PWM API. The spec references
  `daisy::Pwm`. If that exact class does not exist in libDaisy at the
  pinned commit, the closest equivalents are `daisy::TimChannel`,
  `daisy::TimerHandle` with `OutputCompare` channels, or HAL-level
  `HAL_TIM_PWM_Start`. Pick the highest-level libDaisy abstraction that
  works and **note the chosen API in `DaisyLed.h`'s header comment** so
  future hardware changes know which pin/timer constraint applies.
- `SetBrightness(v01)`: clamp to `[0, 1]`, convert to duty cycle, write
  to the PWM channel. Brightness updates must not block; the duty cycle
  register write is a single MMIO store.

## Config.cpp

- Provides definitions for `kButtonPins[4]` and `kLedPins[4]`. Initial
  values are placeholders chosen to compile but flagged to the hardware
  integrator. Use a comment block at the top of the file:

  ```cpp
  // PLACEHOLDER PIN MAP — replace before flashing real hardware.
  // LED pins MUST be on a timer-PWM-capable channel (see SPECIFICATION.md
  // §Hardware Contract). Buttons are active-low with internal pull-up.
  ```

- Suggested placeholders (verify each is timer-PWM-capable on STM32H750
  before committing): buttons on `D7..D10`, LEDs on `D0..D3`. Document
  the timer/channel each LED pin maps to.

## main.cpp

```cpp
#include "daisy_seed.h"
#include "DrumMachine.h"
#include "Config.h"
// instruments, controls, RNG headers...

daisy::DaisySeed hw;
// 4× DaisyButton, 4× DaisyLed, 4× instruments, 1× XorShiftRng, 1× DrumMachine
//   — all as static/global objects (no heap).

void AudioCallback(daisy::AudioHandle::InputBuffer in,
                   daisy::AudioHandle::OutputBuffer out, size_t size) {
    machine.Tick(daisy::System::GetNow());
    for (size_t s = 0; s < size; ++s) {
        const float y = machine.Process();
        out[0][s] = y;
        out[1][s] = y;
    }
}

int main(void) {
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(Config::kAudioBlockSize);
    hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);

    const uint32_t seed = daisy::System::GetUs();   // sampled once
    rng = XorShiftRng(seed);

    // Init each DaisyButton (sample rate from hw), each DaisyLed (timer config),
    // then DrumMachine::Init(48000.f).

    hw.StartAudio(AudioCallback);
    while (true) { /* idle; everything happens in the audio callback */ }
}
```

## Firmware Makefile

- Mirror `Software/GuitarPedal/Makefile`'s structure: `arm-none-eabi-g++`,
  `-std=gnu++20`, `-Ofast`, `-Wall -Wextra -Werror`, `BOOT_SRAM` linker
  config, link against `libdaisy.a` and `libdaisysp.a` from
  `Software/GuitarPedal/dependencies/`.
- Sources: `src/main.cpp`, `src/Config.cpp`, all `src/**/*.cpp` except none —
  unlike `Makefile.test`, the firmware build includes `Daisy*` and `Config.cpp`
  but excludes `test/` entirely.
- Output: `build/DrumMachine.bin`, `build/DrumMachine.elf`.
- `program-dfu` target invoking `dfu-util` with the standard Daisy address
  (copy from `Software/GuitarPedal/Makefile`).

## README.md

The committed README.md (from the spec branch) is already close. Confirm
it has exactly the four sections per spec §README Contents:

1. Build firmware
2. Flash firmware
3. Run host tests
4. Editing the pin map

Do not expand it with architecture overview or marketing copy.

## Acceptance criteria

1. `bash Software/GuitarPedal/ci/build_libs.sh` (one-time prereq) succeeds.
2. `cd Software/DrumMachine && make` produces `build/DrumMachine.bin` with
   no warnings (`-Werror` enforces this).
3. `make -f Makefile.test && ./build/test/run_all` still exits 0.
4. The hardware seam files contain no application logic — they only adapt
   libDaisy primitives to the `IButton`/`ILed` interfaces.

## Manual hardware smoke test (out of scope for AI agents)

Run through spec §Verification step 3 once the board is in hand:
- Each press → LED + drum sound.
- Repeated presses → audible variation.
- 500 ms hold → double-blink confirm; subsequent presses identical;
  hold again restores variation.
- Four-button press → all four sound, no clipping.
- Power-on with finger held → that drum does not fire on boot.

## Notes / risks

- **`daisy::Pwm` verification is the single biggest unknown.** Do this
  preflight before writing `DaisyLed.cpp`. If the pinned libDaisy commit
  does not provide a clean PWM abstraction, escalate to the spec author
  rather than ship a software-PWM fallback (the spec explicitly excludes
  software PWM).
- `daisy::System::GetUs()` is sampled before `DrumMachine::Init()` per spec.
  Make sure `hw.Init()` has run first — `GetUs()` requires the system
  clock, which `DaisySeed::Init()` configures.
- The placeholder pin map is intentionally not the final hardware design.
  Comment it loudly. The hardware integrator owns `Config.cpp` per spec.
- `BOOT_SRAM` matters: the GuitarPedal firmware uses it; copy the linker
  flags exactly. Without it the binary will land at the wrong address and
  the bootloader won't boot it.
