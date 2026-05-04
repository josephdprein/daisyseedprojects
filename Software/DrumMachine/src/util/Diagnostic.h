// Diagnostic.h — lightweight, audio-callback-safe logging hooks.
//
// Why this exists: the firmware is going dark on hardware (codec click on
// boot, then silence). Without instrumentation we can't tell whether the
// audio callback runs, whether button presses register, or whether
// instrument Trig() is reached. This header lets engine code (DrumPad,
// optionally instruments) push tiny event records onto a ring buffer that
// main.cpp drains and prints over USB serial from the idle loop.
//
// Audio-safety: Push() must not allocate, lock, or block. The firmware
// implementation in main.cpp is a lock-free SPSC ring buffer with a single
// producer (the audio callback thread, which is the only place engine code
// runs) and a single consumer (the idle main loop). Overflow drops the
// newest event rather than blocking.
//
// Host build (DRUMMACHINE_HOST_TEST): every Push() collapses to a no-op so
// unit tests aren't slowed down and the linker doesn't need a definition.
//
// This is a debugging tool; remove the diag::Push() calls (or this header)
// once the silent-audio bug is identified.

#pragma once

#include <cstdint>

namespace drum_machine {
namespace diag {

enum PadEvent : uint8_t {
    EvPress = 0,
    EvRelease,
    EvTrig,
    EvBootMaskEngaged,
    EvBootMaskCleared,
};

#if defined(DRUMMACHINE_HOST_TEST)
inline void Push(PadEvent /*ev*/, uint8_t /*pad_id*/) {}
#else
// Defined in src/main.cpp (firmware-only).
void Push(PadEvent ev, uint8_t pad_id);
#endif

}  // namespace diag
}  // namespace drum_machine
