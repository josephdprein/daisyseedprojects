// DaisyButton.cpp — see DaisyButton.h.

#include "DaisyButton.h"

namespace drum_machine {

void DaisyButton::Init(daisy::Pin pin, float sampleRate) {
    // Active-low arcade button with internal pull-up: pressed pulls the line
    // LOW (POLARITY_INVERTED) and the MCU pull-up provides the idle HIGH
    // level (PULL_UP). TYPE_MOMENTARY matches push-to-make button behavior.
    sw_.Init(pin,
             sampleRate,
             daisy::Switch::TYPE_MOMENTARY,
             daisy::Switch::POLARITY_INVERTED,
             daisy::GPIO::Pull::PULLUP);
    is_down_       = false;
    just_pressed_  = false;
    just_released_ = false;
    held_ms_       = 0;
}

void DaisyButton::Update(uint32_t /*nowMs*/) {
    // daisy::Switch keeps its own monotonic time (System::GetNow()) for
    // TimeHeldMs(); the IButton-supplied nowMs is unused here. We still
    // accept it to keep the interface symmetric with MockButton.
    sw_.Debounce();

    is_down_       = sw_.Pressed();
    just_pressed_  = sw_.RisingEdge();
    just_released_ = sw_.FallingEdge();

    // TimeHeldMs returns float milliseconds; truncate to uint32_t. Per
    // IButton contract, HeldMs() is 0 whenever IsDown() is false, which the
    // underlying Switch already guarantees (TimeHeldMs returns 0 if not
    // Pressed()).
    const float held = sw_.TimeHeldMs();
    held_ms_ = held > 0.0f ? static_cast<uint32_t>(held) : 0u;
}

}  // namespace drum_machine
