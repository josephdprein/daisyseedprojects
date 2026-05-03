# Drum Machine Implementation Tasks

These tasks decompose `../SPECIFICATION.md` into ordered, dependency-respecting
chunks. Each file is a self-contained unit of work. Read the spec first; tasks
do not re-state it, they only direct attention.

## Dependency order

Each task assumes the previous tasks are merged. Do not skip ahead.

1. `01-project-scaffold.md` — directory layout, host test harness skeleton.
2. `02-foundation-types-and-interfaces.md` — `PadIndex`, `Config.h` constants,
   all `I*` interface headers, `RandomizationProfile.h`.
3. `03-led-trigger.md` — `LedTrigger` utility (Fire envelope + PulseConfirm).
4. `04-rng-and-mocks.md` — `XorShiftRng` plus `MockButton` / `MockLed` / `MockRng`.
5. `05-drumpad.md` — `DrumPad` state machine, including boot-stuck-button mask.
6. `06-instruments.md` — four DaisySP-backed instruments, randomization tests.
7. `07-mixer.md` — `Mixer` with test-driven gain tuning.
8. `08-drummachine-and-testrig.md` — top-level `DrumMachine` engine and `TestRig`.
9. `09-firmware-integration.md` — `DaisyButton`, `DaisyLed`, `Config.cpp`, `main.cpp`,
   firmware `Makefile`, README polish.

## Conventions

- Every task lists **Files**, **Dependencies**, **Acceptance criteria**,
  **Notes / risks**. Acceptance criteria are testable from a CI agent's seat.
- "Spec §" references point at section headings in `../SPECIFICATION.md`.
- Tests added in a task must keep all earlier tests green; the suite is cumulative.
- Host build: `make -f Makefile.test && ./build/test/run_all` must exit 0
  after every task from 01 onward.
- Firmware build (`make`) is only required to succeed after task 09.

## Resolved planning decisions

- **`Config.h` host-build gating** (task 02): `#if !defined(DRUMMACHINE_HOST_TEST)`
  around the pin externs. Single header preserved.
- **`TestRig::RandomizationEnabled`** (task 08): implementer's call between
  observer-mirror and `friend class TestRig`. Hard rule: keep it simple.
- **HiHat audio-identity test** (task 08): research seedability first; if
  not seedable, fall back to parameter-identity for noise-driven voices.
- **`daisy::Pwm` API** (task 09): preflight check; escalate if missing
  rather than ship software PWM.
- **Sequential implementation:** tasks are executed in order; no further
  splitting of task 08.
