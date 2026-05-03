// MockRng.h — IRng for host tests with optional scripted-sequence override.
//
// Per SPECIFICATION.md §Test Harness and tasks/04-rng-and-mocks.md:
//   - Default behavior: deterministic 32-bit xorshift seeded from the
//     constructor argument. Identical to XorShiftRng so tests can predict
//     outputs without scripting them.
//   - SetSequence(values) overrides: each subsequent NextFloat() pops the
//     front of the queued sequence. When the sequence is exhausted, NextFloat
//     falls back to xorshift output rather than failing — this keeps long
//     randomization tests from tripping if they call NextFloat() more often
//     than the script provides.
//
// MockRng owns its own xorshift state so it can fall back without needing a
// separate IRng to delegate to.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "randomization/IRng.h"

class MockRng final : public IRng {
public:
    explicit MockRng(uint32_t seed);

    // Replace any previously queued sequence with `values`. The next
    // NextFloat() call returns values.front(), then values[1], ... until
    // empty, after which xorshift takes over again.
    void SetSequence(std::vector<float> values);

    // IRng implementation. Returns the next value from the scripted sequence
    // if any remain; otherwise returns the next xorshift float in [0, 1).
    float NextFloat() override;

private:
    uint32_t NextU32();
    float    NextXorshiftFloat();

    uint32_t           state_;
    std::vector<float> sequence_;
    std::size_t        nextIndex_ = 0;  // next entry to pop from sequence_
};
