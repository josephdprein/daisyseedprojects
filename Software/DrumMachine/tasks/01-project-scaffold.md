# 01 — Project scaffold + host test harness

## Goal

Stand up `Software/DrumMachine/` so that `make -f Makefile.test` builds a
single `build/test/run_all` binary and a trivial test passes. No DaisySP
sources are wired in yet; no firmware build yet.

## Dependencies

None. This is the first task.

## Files to create

- `Software/DrumMachine/Makefile.test`
- `Software/DrumMachine/test/support/test_macros.h`
- `Software/DrumMachine/test/main_test.cpp`
- `Software/DrumMachine/test/test_bootstrap.cpp` *(temporary; remains until
  task 02 swaps in real tests, but is harmless to keep as a sanity check)*
- `Software/DrumMachine/.gitignore` — at minimum `build/`

Create empty placeholder directories or `.gitkeep`s for `src/`,
`src/instruments/`, `src/controls/`, `src/randomization/`, `src/util/`,
`test/support/` so the layout is visible.

## What `Makefile.test` must do

- Compiler: `g++ -std=gnu++20 -O2 -g -Wall -Wextra -Werror`.
- Build artifact: `build/test/run_all`.
- Source globs: `test/main_test.cpp`, `test/test_*.cpp`, `test/support/*.cpp`,
  `src/**/*.cpp` (excluding `Config.cpp`, `main.cpp`, and the `Daisy*` controls —
  those are firmware-only). At this point most globs match nothing; that's fine.
- Includes: `src/`, `test/support/`. Do **not** include libDaisy or DaisySP yet
  — that comes in task 06.
- `make clean` removes `build/`.
- `run` target convenience: `make -f Makefile.test run` invokes the binary.

## What `test_macros.h` must provide

Per spec §Test Harness:

- `EXPECT_TRUE(cond)`, `EXPECT_FALSE(cond)`, `EXPECT_EQ(a,b)`, `EXPECT_NEAR(a,b,eps)`,
  `EXPECT_LT/LE/GT/GE`. All throw a `TestFailure` exception on failure carrying
  file, line, and a stringified message.
- `TEST_CASE(name) { … }` — self-registers the case into a static vector held
  inside an inline function (Meyers singleton) so the registry survives
  multi-TU linking under `-O2`.
- The audio assertion helpers (`EXPECT_AUDIO_NOT_SILENT` etc.) are listed in
  spec §Test Harness; **they are not required in task 01** because no audio
  is captured yet. Add them in task 06 alongside `test_instruments.cpp`.

## What `main_test.cpp` must do

- Iterate the registry, run each case inside
  `try { … } catch (const TestFailure&) { … } catch (const std::exception&) { … } catch (...) { … }`.
- Print `passed/failed/total` on the last line.
- Exit with `EXIT_SUCCESS` iff `failed == 0`.

## What `test_bootstrap.cpp` must do

Single `TEST_CASE("bootstrap_runs") { EXPECT_TRUE(true); }`. Confirms the
registry, the macro, and the runner all work end-to-end.

## Acceptance criteria

1. `cd Software/DrumMachine && make -f Makefile.test` succeeds with no warnings.
2. `./build/test/run_all` exits 0 and prints a `passed/failed/total` line with
   `failed == 0` and `total >= 1`.
3. Deliberately failing a test (e.g. `EXPECT_TRUE(false)`) yields a non-zero
   exit code and increments `failed`. Verify locally; revert before commit.
4. `make -f Makefile.test clean` removes `build/`.

## Notes / risks

- The Meyers-singleton registry pattern matters: a plain `static std::vector`
  at namespace scope can be initialized in a different order than the
  `TEST_CASE` self-registrations, leading to a "no tests found" surprise.
- Use `[[noreturn]]` on the assertion helpers' throw paths so the compiler
  doesn't whine about missing returns.
- Keep `test_macros.h` header-only — putting a `.cpp` alongside it complicates
  the Meyers-singleton story.
