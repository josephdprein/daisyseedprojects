// TestRig.cpp — see TestRig.h.

#include "TestRig.h"

#include "Config.h"

#include <array>
#include <cstdint>

namespace {

// Block period in nanoseconds. With the default Config (kAudioBlockSize=48,
// kSampleRate=48000) this is exactly 1,000,000 ns (= 1 ms); for retuned block
// sizes the rounding accumulates in subMsNs_ on the rig.
constexpr uint64_t BlockPeriodNs() {
    // 1e9 nanoseconds per second; samples per block / samples per second
    // gives seconds per block. Multiply through in integer arithmetic to avoid
    // float drift.
    return static_cast<uint64_t>(Config::kAudioBlockSize) * 1'000'000'000ull
         / static_cast<uint64_t>(Config::kSampleRate);
}

constexpr uint64_t kBlockPeriodNs = BlockPeriodNs();

}  // namespace

TestRig::TestRig() : TestRig(1) {}

TestRig::TestRig(uint32_t rngSeed)
    : bass_(),
      snare_(),
      hihat_(),
      resonator_(),
      rng_(rngSeed),
      machine_(
          std::array<IButton*, kNumPads>{
              &buttons_[0], &buttons_[1], &buttons_[2], &buttons_[3]},
          std::array<ILed*, kNumPads>{
              &leds_[0], &leds_[1], &leds_[2], &leds_[3]},
          std::array<IInstrument*, kNumPads>{
              &bass_, &snare_, &hihat_, &resonator_},
          rng_) {
    // Default per-pad randomization state matches DrumPad's default: enabled.
    for (std::size_t i = 0; i < kNumPads; ++i) {
        randEnabled_[i] = true;
    }
    // Initialize the engine at the spec sample rate. Init is idempotent so a
    // test that wants to "panic reset" can call rig.Machine().Init(...) again
    // — but TestRig doesn't expose the engine, so tests use AdvanceMs etc.
    machine_.Init(Config::kSampleRate);
}

void TestRig::PressButton(::PadIndex pad) {
    // Script the edge at the current sim time. The next AdvanceMs / CaptureAudio
    // tick will pick it up via MockButton::Update.
    buttons_[Idx(pad)].ScriptPress(nowMs_);
}

void TestRig::ReleaseButton(::PadIndex pad) {
    buttons_[Idx(pad)].ScriptRelease(nowMs_);
}

void TestRig::HoldButton(::PadIndex pad, uint32_t ms) {
    PressButton(pad);
    AdvanceMs(ms);
    ReleaseButton(pad);
    // Observer-mirror: any hold whose duration crosses Config::kHoldThresholdMs
    // produces a randomization toggle inside DrumPad. We mirror that flip here
    // so RandomizationEnabled(pad) reflects engine state without a friend
    // declaration. See TestRig.h for why this is sufficient.
    if (ms >= Config::kHoldThresholdMs) {
        randEnabled_[Idx(pad)] = !randEnabled_[Idx(pad)];
    }
}

void TestRig::AdvanceMs(uint32_t ms) {
    if (ms == 0) {
        return;
    }
    // Advance in control-tick increments until we've consumed at least `ms`
    // worth of sim time. We use a target nanosecond timestamp so retuned
    // block sizes don't drift.
    const uint64_t targetNs = static_cast<uint64_t>(nowMs_) * 1'000'000ull
                            + subMsNs_
                            + static_cast<uint64_t>(ms) * 1'000'000ull;
    while ((static_cast<uint64_t>(nowMs_) * 1'000'000ull + subMsNs_) < targetNs) {
        TickOnce(/*buf=*/nullptr);
    }
}

std::vector<float> TestRig::CaptureAudio(uint32_t ms) {
    std::vector<float> buf;
    if (ms == 0) {
        return buf;
    }
    // Length is approximately ms * kSampleRate / 1000 samples; we reserve up
    // to one extra block to absorb the loop's last partial step.
    const std::size_t expectedSamples =
        static_cast<std::size_t>(ms)
        * static_cast<std::size_t>(Config::kSampleRate)
        / 1000u;
    buf.reserve(expectedSamples + Config::kAudioBlockSize);

    const uint64_t targetNs = static_cast<uint64_t>(nowMs_) * 1'000'000ull
                            + subMsNs_
                            + static_cast<uint64_t>(ms) * 1'000'000ull;
    while ((static_cast<uint64_t>(nowMs_) * 1'000'000ull + subMsNs_) < targetNs) {
        TickOnce(&buf);
    }
    return buf;
}

void TestRig::TickOnce(std::vector<float>* buf) {
    // Tag MockLeds with the current time so SetBrightness samples land with
    // the right timestamp.
    for (auto& led : leds_) {
        led.SetNow(nowMs_);
    }
    machine_.Tick(nowMs_);
    if (buf != nullptr) {
        // One audio block per control tick.
        for (std::size_t s = 0; s < Config::kAudioBlockSize; ++s) {
            buf->push_back(machine_.Process());
        }
    }
    // Advance sim clock by exactly one block period (in ns), then carry to ms.
    subMsNs_ += kBlockPeriodNs;
    while (subMsNs_ >= 1'000'000ull) {
        subMsNs_ -= 1'000'000ull;
        ++nowMs_;
    }
}

float TestRig::LedBrightness(::PadIndex pad) const {
    return leds_[Idx(pad)].Last();
}

bool TestRig::RandomizationEnabled(::PadIndex pad) const {
    return randEnabled_[Idx(pad)];
}

const std::vector<std::pair<uint32_t, float>>&
TestRig::LedHistory(::PadIndex pad) const {
    return leds_[Idx(pad)].BrightnessHistory();
}
