// test_rng.cpp — unit tests for XorShiftRng and MockRng.
//
// Per tasks/04-rng-and-mocks.md "Test cases":
//   1. Determinism: two XorShiftRng(42) instances produce the same first
//      100 values (xorshift state is fully determined by the seed).
//   2. Range: 1000 calls all satisfy 0.0f <= v < 1.0f (upper bound is
//      strictly exclusive).
//   3. MockRng sequence: SetSequence({0.1, 0.2, 0.3}) returns those three
//      values in order, then falls back to xorshift output (in [0, 1)).

#include "MockRng.h"
#include "randomization/XorShiftRng.h"
#include "test_macros.h"

TEST_CASE("XorShiftRng is deterministic for a given seed") {
    XorShiftRng a(42);
    XorShiftRng b(42);
    for (int i = 0; i < 100; ++i) {
        const float va = a.NextFloat();
        const float vb = b.NextFloat();
        // Bit-exact equality is the right comparison: both generators run
        // the identical xorshift state machine with identical inputs.
        EXPECT_EQ(va, vb);
    }
}

TEST_CASE("XorShiftRng output is in [0, 1)") {
    XorShiftRng rng(12345);
    for (int i = 0; i < 1000; ++i) {
        const float v = rng.NextFloat();
        EXPECT_GE(v, 0.0f);
        EXPECT_LT(v, 1.0f);
    }
}

TEST_CASE("MockRng SetSequence returns values in order then falls back") {
    MockRng rng(7);
    rng.SetSequence({0.1f, 0.2f, 0.3f});

    EXPECT_EQ(rng.NextFloat(), 0.1f);
    EXPECT_EQ(rng.NextFloat(), 0.2f);
    EXPECT_EQ(rng.NextFloat(), 0.3f);

    // Sequence exhausted — fourth call must fall back to xorshift output and
    // therefore land in [0, 1) per the IRng contract.
    const float fallback = rng.NextFloat();
    EXPECT_GE(fallback, 0.0f);
    EXPECT_LT(fallback, 1.0f);
}
