// PadIndex.h — enumerates the four drum pads.
//
// Per SPECIFICATION.md §Type Definitions. Values are fixed (Bass=0, Snare=1,
// HiHat=2, Resonator=3) so that std::array<T, PadIndex::Count> indexed by a
// PadIndex casts cleanly. `Count` is intentionally a member so that callers
// can size arrays via `static_cast<size_t>(PadIndex::Count)`.

#pragma once

#include <cstdint>

enum class PadIndex : uint8_t {
    Bass      = 0,
    Snare     = 1,
    HiHat     = 2,
    Resonator = 3,
    Count     = 4
};
