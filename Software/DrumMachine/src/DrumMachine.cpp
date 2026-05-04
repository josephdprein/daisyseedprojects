// DrumMachine.cpp — see DrumMachine.h.

#include "DrumMachine.h"

#include "Config.h"

namespace drum_machine {

namespace {

// Helper: build the array of DrumPads in PadIndex order. Avoids ugly
// brace-init repetition in the constructor's member-initializer list.
std::array<DrumPad, DrumMachine::kNumPads>
MakePads(const std::array<IButton*,     DrumMachine::kNumPads>& buttons,
         const std::array<ILed*,        DrumMachine::kNumPads>& leds,
         const std::array<IInstrument*, DrumMachine::kNumPads>& instruments,
         IRng&                                                  rng) {
    return {
        DrumPad(*buttons[0], *leds[0], *instruments[0], rng),
        DrumPad(*buttons[1], *leds[1], *instruments[1], rng),
        DrumPad(*buttons[2], *leds[2], *instruments[2], rng),
        DrumPad(*buttons[3], *leds[3], *instruments[3], rng),
    };
}

}  // namespace

DrumMachine::DrumMachine(
    std::array<IButton*,     kNumPads> buttons,
    std::array<ILed*,        kNumPads> leds,
    std::array<IInstrument*, kNumPads> instruments,
    IRng&                              rng)
    : instruments_(instruments),
      pads_(MakePads(buttons, leds, instruments, rng)),
      mixer_(instruments) {}

void DrumMachine::Init(float sampleRate) {
    // Per spec §DrumMachine API: Init is idempotent. Forward to each
    // instrument and reset every pad's state (LedTrigger cleared, hold
    // detection re-armed, boot-stuck mask re-applied on the next Tick).
    for (std::size_t i = 0; i < kNumPads; ++i) {
        instruments_[i]->Init(sampleRate, Config::kDefaultRandomizationDepth);
        pads_[i].Init();
    }
}

void DrumMachine::Tick(uint32_t nowMs) {
    // PadIndex order: Bass, Snare, HiHat, Resonator. Pads pressed in the same
    // tick all fire that tick because each Tick() calls instrument.Trig()
    // synchronously on a JustPressed edge.
    for (std::size_t i = 0; i < kNumPads; ++i) {
        pads_[i].Tick(nowMs, static_cast<uint8_t>(i));
    }
}

float DrumMachine::Process() {
    return mixer_.Process();
}

}  // namespace drum_machine
