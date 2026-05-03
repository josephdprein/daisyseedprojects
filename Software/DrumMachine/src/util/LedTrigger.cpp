// LedTrigger.cpp — implementation of the linear Fire envelope and the
// PulseConfirm blink pattern. See header for spec references.

#include "util/LedTrigger.h"

#include <cstdint>

#include "Config.h"

namespace drum_machine {

namespace {

// Pulse pattern segment count: each repetition is (on, gap); the last gap of
// the last repetition is the trailing "off" segment of the spec's
// "on / gap / on / off" pattern, so the total length is just
// kPulseConfirmCount * (kPulseConfirmOnMs + kPulseConfirmGapMs).
constexpr uint32_t PulseUnitMs() {
    return Config::kPulseConfirmOnMs + Config::kPulseConfirmGapMs;
}

constexpr uint32_t PulseTotalMs() {
    return static_cast<uint32_t>(Config::kPulseConfirmCount) * PulseUnitMs();
}

}  // namespace

void LedTrigger::Fire(uint32_t nowMs) {
    mode_  = Mode::Fire;
    start_ = nowMs;
}

void LedTrigger::PulseConfirm(uint32_t nowMs) {
    // PulseConfirm wins: any in-flight Fire envelope is overwritten — the
    // pulse pattern alone drives the LED until it completes.
    mode_  = Mode::Pulse;
    start_ = nowMs;
}

void LedTrigger::Reset() {
    mode_  = Mode::Idle;
    start_ = 0;
}

float LedTrigger::Brightness(uint32_t nowMs) const {
    switch (mode_) {
        case Mode::Idle:
            return 0.0f;

        case Mode::Fire: {
            // Guard against a caller passing an nowMs older than the start;
            // treat as pre-start (brightness 0 — the ramp begins exactly at
            // start_).
            if (nowMs < start_) {
                return 0.0f;
            }
            const uint32_t elapsed = nowMs - start_;
            if (elapsed < Config::kLedAttackMs) {
                // Linear 0 → 1 over [0, kLedAttackMs).
                return static_cast<float>(elapsed) /
                       static_cast<float>(Config::kLedAttackMs);
            }
            const uint32_t decayElapsed = elapsed - Config::kLedAttackMs;
            if (decayElapsed < Config::kLedDecayMs) {
                // Linear 1 → 0 over [kLedAttackMs, kLedAttackMs+kLedDecayMs).
                return 1.0f - static_cast<float>(decayElapsed) /
                                  static_cast<float>(Config::kLedDecayMs);
            }
            // Past decay: exactly zero (spec: "After decay completes the
            // value is exactly 0").
            return 0.0f;
        }

        case Mode::Pulse: {
            if (nowMs < start_) {
                return 0.0f;
            }
            const uint32_t elapsed = nowMs - start_;
            if (elapsed >= PulseTotalMs()) {
                return 0.0f;
            }
            // Within the pattern: each unit is (on for kPulseConfirmOnMs,
            // gap for kPulseConfirmGapMs).
            const uint32_t intoUnit = elapsed % PulseUnitMs();
            return (intoUnit < Config::kPulseConfirmOnMs) ? 1.0f : 0.0f;
        }
    }
    // Unreachable — all enum values handled.
    return 0.0f;
}

}  // namespace drum_machine
