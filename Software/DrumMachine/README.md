# Four-Button Drum Machine

Standalone Daisy Seed firmware: four arcade buttons, four drum voices, per-pad randomization. See `SPECIFICATION.md` for the design contract.

## Build firmware

Prereq: built `libDaisy` and `DaisySP` static libs. From the repo root:

```
bash Software/GuitarPedal/ci/build_libs.sh   # one-time
cd Software/DrumMachine
make
```

Output: `build/DrumMachine.bin`.

## Flash firmware

Hold the Daisy Seed `BOOT` button, tap `RESET`, release `BOOT` (the Seed is now in DFU mode), then:

```
make program-dfu
```

If `dfu-util` reports "no DFU capable USB device available", re-enter DFU mode and retry — the bootloader window is short.

## Run host tests

The tests build natively (no Daisy hardware, no cross-compiler):

```
cd Software/DrumMachine
make -f Makefile.test
./build/test/run_all
```

Output ends with a `passed/failed/total` line. Exit code is `0` iff `failed == 0`.

## Editing the pin map

All pin assignments live in `src/Config.cpp`. **LED pins must be timer-PWM-capable** (see Hardware Contract in `SPECIFICATION.md`). No other file should reference Daisy pins directly — if you need to change wiring, only `Config.cpp` should change.
