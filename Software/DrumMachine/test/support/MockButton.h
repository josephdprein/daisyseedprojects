// MockButton.h — script-driven IButton for host tests.
//
// Per SPECIFICATION.md §Test Harness and tasks/04-rng-and-mocks.md:
//   - ScriptPress(atMs) / ScriptRelease(atMs) append events to an ordered
//     queue. Multiple events allowed.
//   - Update(nowMs) consumes any events with atMs <= nowMs since the last
//     Update, applying them to the internal "down" state in order.
//   - JustPressed/JustReleased describe transitions during the most recent
//     Update call only — two consecutive Updates with the same nowMs both
//     return false (these are not standing edges).
//   - HeldMs() returns 0 when not down, otherwise nowMs - lastPressMs.
//   - Out-of-order script timestamps (atMs going backwards) are undefined
//     and tests must not depend on that ordering.

#pragma once

#include <cstdint>
#include <vector>

#include "controls/IButton.h"

class MockButton final : public IButton {
public:
    MockButton() = default;

    // Queue a press event at the given simulated time. Events with the same
    // atMs are processed in insertion order.
    void ScriptPress(uint32_t atMs);

    // Queue a release event at the given simulated time.
    void ScriptRelease(uint32_t atMs);

    // IButton implementation.
    void     Update(uint32_t nowMs) override;
    bool     IsDown() const override;
    bool     JustPressed() const override;
    bool     JustReleased() const override;
    uint32_t HeldMs() const override;

private:
    struct Event {
        uint32_t atMs;
        bool     pressed;  // true → press, false → release
    };

    std::vector<Event> script_;
    std::size_t        nextEvent_   = 0;       // index into script_
    bool               isDown_      = false;
    bool               justPressed_ = false;
    bool               justReleased_ = false;
    uint32_t           lastPressMs_ = 0;
    uint32_t           nowMs_       = 0;
};
