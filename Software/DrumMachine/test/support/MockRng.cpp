// MockRng.cpp — implementation of the test RNG with sequence override.

#include "MockRng.h"

#include <utility>

namespace {

// Same fallback constants as XorShiftRng so MockRng's default-mode output
// matches XorShiftRng bit-for-bit when seeded identically (tests rely on
// this equivalence for determinism checks across the codebase).
constexpr uint32_t kFallbackSeed = 2463534242u;
constexpr float    kFloatScale   = 1.0f / 16777216.0f;  // 1 / 2^24

}  // namespace

MockRng::MockRng(uint32_t seed)
    : state_(seed == 0 ? kFallbackSeed : seed) {}

void MockRng::SetSequence(std::vector<float> values) {
    sequence_  = std::move(values);
    nextIndex_ = 0;
}

uint32_t MockRng::NextU32() {
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;
    return x;
}

float MockRng::NextXorshiftFloat() {
    const uint32_t bits24 = NextU32() >> 8;
    return static_cast<float>(bits24) * kFloatScale;
}

float MockRng::NextFloat() {
    if (nextIndex_ < sequence_.size()) {
        return sequence_[nextIndex_++];
    }
    // Sequence exhausted — fall back to xorshift output. Documented behavior
    // so randomization tests aren't fragile when they over-consume.
    return NextXorshiftFloat();
}
