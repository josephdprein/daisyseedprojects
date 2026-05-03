// IRng.h — uniform random number source abstraction.
//
// Per SPECIFICATION.md §Core Interfaces. Concrete impls: XorShiftRng (seeded
// from daisy::System::GetUs() at boot) for firmware, MockRng (deterministic
// xorshift + scripted-sequence override) for host tests.

#pragma once

class IRng {
public:
    virtual ~IRng() = default;

    // Returns a uniformly distributed float in the half-open interval
    // [0, 1). Implementations must never return exactly 1.0f.
    virtual float NextFloat() = 0;
};
