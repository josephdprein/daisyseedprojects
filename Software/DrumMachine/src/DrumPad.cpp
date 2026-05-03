// DrumPad.cpp — implementation of the per-pad state machine.
// See header for spec references. The pseudocode in SPECIFICATION.md
// §DrumPad Behavior is the source of truth — keep this file structurally
// aligned with that pseudocode so reviewers can diff line-for-line.

#include "DrumPad.h"

#include "Config.h"

namespace drum_machine {

DrumPad::DrumPad(IButton& button, ILed& led, IInstrument& instrument,
                 IRng& rng)
    : button_(button), led_(led), instrument_(instrument), rng_(rng) {}

void DrumPad::Init() {
    ledTrigger_.Reset();
    holdToggleArmed_         = false;
    pressMaskedUntilRelease_ = false;
    // Re-apply the boot-stuck mask check on the next Tick: if the button is
    // already down when Tick runs, that Tick will set the mask flag rather
    // than treating the down state as a fresh press.
    firstTickAfterInit_ = true;
}

void DrumPad::Tick(uint32_t nowMs) {
    button_.Update(nowMs);

    // Boot-stuck-button mask: if the very first Tick after Init() sees the
    // button already "down", suppress press semantics until we observe at
    // least one release. Without this, holding any button at power-on (or
    // re-running Init() as a panic reset while a button is pressed) would
    // fire that drum and consume a Randomize() roll spuriously — and on a
    // four-button device with all buttons held it would fire all four. The
    // mask is cleared by JustReleased(), after which a subsequent press
    // triggers normally.
    if (firstTickAfterInit_ && button_.IsDown()) {
        pressMaskedUntilRelease_ = true;
    }
    // Flip the first-tick flag now, regardless of mask outcome — task 05
    // brief: "flip it to false at the end of the first Tick regardless of
    // mask outcome." (We do it pre-LED-write so an early `return` below
    // doesn't leave us stuck thinking it's still the first tick.)
    firstTickAfterInit_ = false;

    if (pressMaskedUntilRelease_) {
        if (button_.JustReleased()) {
            pressMaskedUntilRelease_ = false;
        }
        // While masked: no Trig, no Randomize, no hold-toggle, no LED
        // activity. The LED stays dark because ledTrigger_ remains in its
        // Idle state from Init().
        led_.SetBrightness(ledTrigger_.Brightness(nowMs));
        return;
    }

    if (button_.JustPressed()) {
        // Spec §DrumPad Behavior: Trig fires immediately on press, then we
        // re-randomize for the *next* press. holdToggleArmed is re-armed on
        // every fresh press (not just on release) so that a press → toggle →
        // release → press sequence can toggle again on the new hold.
        instrument_.Trig();
        ledTrigger_.Fire(nowMs);
        if (randomizationEnabled_) {
            instrument_.Randomize(rng_);
        }
        holdToggleArmed_ = true;
    }

    if (button_.IsDown() && holdToggleArmed_ &&
        button_.HeldMs() >= Config::kHoldThresholdMs) {
        randomizationEnabled_ = !randomizationEnabled_;
        // PulseConfirm wins over the active Fire envelope (see LedTrigger
        // and spec §PulseConfirm vs active envelope).
        ledTrigger_.PulseConfirm(nowMs);
        holdToggleArmed_ = false;  // don't re-toggle on this same hold.
    }

    if (button_.JustReleased()) {
        holdToggleArmed_ = false;
    }

    led_.SetBrightness(ledTrigger_.Brightness(nowMs));
}

}  // namespace drum_machine
