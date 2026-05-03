// Config.h — constants and pin map declarations for the DrumMachine firmware.
//
// Per SPECIFICATION.md §Hardware Contract: every tunable lives here so the
// hardware integrator can adjust pins / timings without touching app code.
//
// Host-build note: `daisy::Pin` ships with libDaisy and is unavailable on the
// native test build. Per task 02 brief, the pin externs are gated behind
// DRUMMACHINE_HOST_TEST so the same Config.h participates in both builds.
// Makefile.test passes -DDRUMMACHINE_HOST_TEST; the firmware Makefile does
// not. Definitions of the pin arrays live in src/Config.cpp (firmware-only).

#pragma once

#include <cstddef>
#include <cstdint>

#include "PadIndex.h"

namespace Config {

// ---- Audio ------------------------------------------------------------------
constexpr float  kSampleRate     = 48000.0f;  // Daisy Seed default.
constexpr size_t kAudioBlockSize = 48;        // 1 ms control tick at 48 kHz.

// ---- Press-and-hold randomization toggle ------------------------------------
constexpr uint32_t kHoldThresholdMs = 500;

// ---- LED envelope -----------------------------------------------------------
// Linear attack/decay shape produced by LedTrigger::Fire (see spec §LED env).
constexpr uint32_t kLedAttackMs = 5;
constexpr uint32_t kLedDecayMs  = 120;

// ---- PulseConfirm pattern (toggle-state confirmation blink) -----------------
// Shape: on, gap, on, off — repeated kPulseConfirmCount times.
constexpr uint32_t kPulseConfirmOnMs  = 50;
constexpr uint32_t kPulseConfirmGapMs = 50;
constexpr uint8_t  kPulseConfirmCount = 2;

// ---- Randomization ----------------------------------------------------------
// Init-time only depth (no runtime control surface — see spec §Instruments).
constexpr float kDefaultRandomizationDepth = 0.5f;

}  // namespace Config

// ---- Hardware pin map (firmware-only) ---------------------------------------
// `daisy::Pin` is a libDaisy type that has no host equivalent, so the pin
// externs are excluded from the host test build. Definitions live in
// src/Config.cpp, which is also excluded from Makefile.test.
#if !defined(DRUMMACHINE_HOST_TEST)
#include "daisy_seed.h"

namespace Config {
extern const daisy::Pin kButtonPins[static_cast<size_t>(PadIndex::Count)];
extern const daisy::Pin kLedPins[static_cast<size_t>(PadIndex::Count)];
}  // namespace Config
#endif  // !DRUMMACHINE_HOST_TEST
