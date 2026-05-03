// IButton.h — debounced button input abstraction.
//
// Per SPECIFICATION.md §Core Interfaces. Concrete impls: DaisyButton (wraps
// daisy::Switch) for firmware, MockButton (script-driven) for host tests.
//
// Edge semantics are sampled per Update() call: JustPressed/JustReleased
// reflect transitions observed during the most recent Update() and remain
// stable until the next Update(). HeldMs() returns 0 when not currently down.

#pragma once

#include <cstdint>

class IButton {
public:
    virtual ~IButton() = default;

    // Sample the underlying input and update edge / hold state. Must be
    // called once per control tick, with a monotonic millisecond clock.
    virtual void Update(uint32_t nowMs) = 0;

    // True while the button is physically down (post-debounce).
    virtual bool IsDown() const = 0;

    // True iff the most recent Update() observed a rising edge (released
    // → pressed). Stays true until the next Update() call.
    virtual bool JustPressed() const = 0;

    // True iff the most recent Update() observed a falling edge (pressed
    // → released). Stays true until the next Update() call.
    virtual bool JustReleased() const = 0;

    // Milliseconds the button has been continuously held since the most
    // recent press. Contract: returns 0 whenever IsDown() is false.
    // Both DaisyButton and MockButton must honor this.
    virtual uint32_t HeldMs() const = 0;
};
