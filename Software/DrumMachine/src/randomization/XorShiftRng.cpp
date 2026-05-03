// XorShiftRng.cpp — implementation of the 32-bit xorshift RNG.

#include "randomization/XorShiftRng.h"

namespace {

// Non-zero seed substituted when the caller passes 0. Any non-zero constant
// works; this value is George Marsaglia's customary xorshift default and
// matches what is widely used in reference implementations.
constexpr uint32_t kFallbackSeed = 2463534242u;

// Conversion constant for [0, 1) floats. We take the high 24 bits of the
// 32-bit state (the IEEE-754 single-precision mantissa is 23 bits + the
// implicit 1, giving 24 bits of precision) and divide by 2^24. The largest
// possible numerator is (2^24 - 1), so the result is strictly < 1.0f.
constexpr float kFloatScale = 1.0f / 16777216.0f;  // 1 / 2^24

}  // namespace

XorShiftRng::XorShiftRng(uint32_t seed)
    : state_(seed == 0 ? kFallbackSeed : seed) {}

uint32_t XorShiftRng::NextU32() {
    // Standard 32-bit xorshift (Marsaglia 2003): periods length 2^32 - 1.
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;
    return x;
}

float XorShiftRng::NextFloat() {
    const uint32_t bits24 = NextU32() >> 8;  // top 24 bits → 0 .. 2^24 - 1
    return static_cast<float>(bits24) * kFloatScale;
}
