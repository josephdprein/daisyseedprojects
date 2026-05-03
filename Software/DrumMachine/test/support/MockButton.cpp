// MockButton.cpp — implementation of the script-driven test button.

#include "MockButton.h"

void MockButton::ScriptPress(uint32_t atMs) {
    script_.push_back(Event{atMs, true});
}

void MockButton::ScriptRelease(uint32_t atMs) {
    script_.push_back(Event{atMs, false});
}

void MockButton::Update(uint32_t nowMs) {
    nowMs_ = nowMs;

    // Edges describe transitions observed during *this* Update only; clear
    // them up front so two consecutive Updates with the same nowMs and no
    // new events both report false.
    justPressed_  = false;
    justReleased_ = false;

    // Drain every queued event whose atMs has been reached. Out-of-order
    // scripting (atMs going backwards) is undefined per the task brief, so
    // we walk the queue in insertion order and simply compare against nowMs.
    while (nextEvent_ < script_.size() &&
           script_[nextEvent_].atMs <= nowMs) {
        const Event& ev = script_[nextEvent_++];
        if (ev.pressed) {
            if (!isDown_) {
                isDown_       = true;
                justPressed_  = true;
                lastPressMs_  = ev.atMs;
            }
            // Press while already down: idempotent — no edge fires.
        } else {
            if (isDown_) {
                isDown_       = false;
                justReleased_ = true;
            }
            // Release while already up: idempotent — no edge fires.
        }
    }
}

bool MockButton::IsDown() const { return isDown_; }

bool MockButton::JustPressed() const { return justPressed_; }

bool MockButton::JustReleased() const { return justReleased_; }

uint32_t MockButton::HeldMs() const {
    if (!isDown_) {
        return 0;
    }
    // Defensive: if nowMs ever drifted before lastPressMs (e.g. an
    // out-of-order script), report 0 rather than wrapping around.
    if (nowMs_ < lastPressMs_) {
        return 0;
    }
    return nowMs_ - lastPressMs_;
}
