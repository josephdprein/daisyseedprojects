// MockLed.h — recording ILed for host tests.
//
// Per SPECIFICATION.md §Test Harness and tasks/04-rng-and-mocks.md:
//   - SetBrightness(v) clamps v to [0, 1] and pushes (currentNowMs,
//     clampedValue) onto an internal history vector. The current "now"
//     timestamp is supplied externally via SetNow(nowMs); the test rig
//     calls SetNow before each control tick so MockLed never has to
//     reach into a global clock.
//   - BrightnessHistory() returns a const reference to the underlying
//     vector — tests inspect it directly.
//   - Last() returns the most recent value, or 0.0f if no SetBrightness
//     has been called yet.
//
// Allocation: SetBrightness may grow the history vector — that's accepted
// because this class is test-only.

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "controls/ILed.h"

class MockLed final : public ILed {
public:
    MockLed() = default;

    // Record (currentNowMs, clamp(v01, 0, 1)) on the history.
    void SetBrightness(float v01) override;

    // Set the timestamp tagged onto the next SetBrightness sample. Tests
    // (typically the TestRig) call this before each control tick.
    void SetNow(uint32_t nowMs);

    // Full history of (nowMs, brightness) samples in call order.
    const std::vector<std::pair<uint32_t, float>>& BrightnessHistory() const;

    // Most recent brightness value, or 0.0f if no samples have been recorded.
    float Last() const;

private:
    std::vector<std::pair<uint32_t, float>> history_;
    uint32_t                                nowMs_ = 0;
};
