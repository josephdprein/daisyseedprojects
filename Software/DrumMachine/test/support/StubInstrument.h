// StubInstrument.h — header-only IInstrument stub for DrumPad tests.
//
// Per SPECIFICATION.md §Decoupling smoke test and tasks/05-drumpad.md:
//   - Records the order and counts of Trig() and Randomize() calls so tests
//     can assert ordering (e.g. [TRIG, RANDOMIZE]) without any real DaisySP
//     voice in the picture.
//   - Captures the IRng* most recently passed to Randomize so tests can
//     verify the wiring.
//   - Process() returns 0.0f — DrumPad never calls it; this stub is for the
//     control-path tests only.
//
// Allocation: events_ is a std::vector that may grow; this is acceptable
// because StubInstrument is test-only.

#pragma once

#include <cstddef>
#include <vector>

#include "instruments/IInstrument.h"
#include "randomization/IRng.h"

class StubInstrument final : public IInstrument {
public:
    enum class Event {
        Trig,
        Randomize,
    };

    StubInstrument() = default;

    void Init(float /*sampleRate*/, float /*depth01*/) override {
        // No-op: this stub has no internal voice state to initialize. We
        // still record nothing here, since DrumPad does not call Init() on
        // the instrument (DrumMachine does, in task 08). Callers who want to
        // explicitly verify Init wiring in a future test can extend this.
    }

    void Trig() override {
        ++trigCount_;
        events_.push_back(Event::Trig);
    }

    float Process() override { return 0.0f; }

    void Randomize(IRng& rng) override {
        ++randomizeCount_;
        lastRng_ = &rng;
        events_.push_back(Event::Randomize);
    }

    // Inspectors ----------------------------------------------------------
    std::size_t TrigCount()      const { return trigCount_; }
    std::size_t RandomizeCount() const { return randomizeCount_; }
    IRng*       LastRng()        const { return lastRng_; }

    const std::vector<Event>& Events() const { return events_; }

    // Convenience: clear recorded history without touching the lastRng_
    // pointer (useful between sub-phases of a single test).
    void Reset() {
        trigCount_      = 0;
        randomizeCount_ = 0;
        events_.clear();
    }

private:
    std::size_t        trigCount_      = 0;
    std::size_t        randomizeCount_ = 0;
    IRng*              lastRng_        = nullptr;
    std::vector<Event> events_;
};
