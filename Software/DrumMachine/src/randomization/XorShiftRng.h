// XorShiftRng.h — deterministic 32-bit xorshift implementation of IRng.
//
// Per SPECIFICATION.md §Core Interfaces and tasks/04-rng-and-mocks.md:
//   - Standard 32-bit xorshift: x ^= x<<13; x ^= x>>17; x ^= x<<5.
//   - Constructor takes a uint32_t seed. A seed of 0 collapses xorshift
//     (every step yields 0), so a non-zero default is substituted in that
//     case. The substitution is documented at the implementation site.
//   - NextFloat() returns a value in the half-open interval [0, 1). The
//     conversion uses a 24-bit slice divided by 2^24 so the upper bound is
//     strictly exclusive (the largest representable value is just under 1.0f).
//
// Audio-path contract: NextFloat() performs no heap allocation and no system
// calls — three integer ops plus a single float divide.

#pragma once

#include <cstdint>

#include "randomization/IRng.h"

class XorShiftRng final : public IRng {
public:
    // Construct with a 32-bit seed. seed == 0 is replaced internally with a
    // fixed non-zero default to keep the generator from collapsing.
    explicit XorShiftRng(uint32_t seed);

    // Returns a uniformly distributed float in [0, 1). Never returns 1.0f.
    float NextFloat() override;

private:
    // Advance the state once and return the new 32-bit word.
    uint32_t NextU32();

    uint32_t state_;
};
