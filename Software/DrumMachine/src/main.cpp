// main.cpp — firmware entry point for the four-button drum machine.
//
// Per SPECIFICATION.md §DrumMachine API and tasks/09-firmware-integration.md:
//   - All hardware-facing objects (buttons, LEDs, instruments, RNG, engine)
//     are file-scope statics — no heap allocation anywhere in the firmware.
//   - The audio callback runs at block rate: one DrumMachine::Tick per block,
//     then DrumMachine::Process for every sample. Output is mono, duplicated
//     into both stereo channels (see spec §Audio Configuration).
//   - The RNG seed is sampled from System::GetUs() *after* hw.Init() — the
//     system clock that drives GetUs() is configured by DaisySeed::Init().
//
// Pin map and the four-channel TIM4 PWM peripheral live in src/Config.cpp;
// no other source file in this project references daisy::Pin directly.

#include <array>
#include <cstddef>
#include <cstdint>

#include "daisy_seed.h"
#include "per/pwm.h"

#include "Config.h"
#include "DrumMachine.h"
#include "PadIndex.h"
#include "controls/DaisyButton.h"
#include "controls/DaisyLed.h"
#include "controls/IButton.h"
#include "controls/ILed.h"
#include "instruments/BassDrum.h"
#include "instruments/HiHat.h"
#include "instruments/IInstrument.h"
#include "instruments/Resonator.h"
#include "instruments/Snare.h"
#include "randomization/XorShiftRng.h"

using drum_machine::DaisyButton;
using drum_machine::DaisyLed;
using drum_machine::DrumMachine;

// ---- Hardware singletons ---------------------------------------------------
// Daisy Seed BSP. Configure() and Init() are called from main().
static daisy::DaisySeed hw;

// One PWM peripheral (TIM4) drives all four LED channels — channels 1..4 map
// to PB6/PB7/PB8/PB9 = D13/D14/D11/D12 respectively (see Config.cpp).
static daisy::PWMHandle led_pwm;

// ---- Engine plumbing -------------------------------------------------------
constexpr std::size_t kNumPads =
    static_cast<std::size_t>(::PadIndex::Count);

static DaisyButton g_buttons[kNumPads];
static DaisyLed    g_leds[kNumPads];

static drum_machine::BassDrum  g_bass_drum;
static drum_machine::Snare     g_snare_drum;
static drum_machine::HiHat     g_hi_hat;
static drum_machine::Resonator g_resonator;

// XorShiftRng has a non-zero default seed; it is reseeded in main() with
// System::GetUs() before DrumMachine::Init() runs, per spec §Hardware Contract.
static XorShiftRng g_rng(1u);

// Pointer tables passed to DrumMachine. Address-of a static array element is
// a constant expression, so these tables are fully resolved at static-init
// time — no run-time wiring needed before the DrumMachine constructor runs.
// (DrumMachine stores the pointer arrays by value and the underlying DrumPad
// objects hold references, which is why we can't reassign the engine later.)
static std::array<IButton*, kNumPads> g_button_ptrs = {
    &g_buttons[0], &g_buttons[1], &g_buttons[2], &g_buttons[3]
};
static std::array<ILed*, kNumPads> g_led_ptrs = {
    &g_leds[0], &g_leds[1], &g_leds[2], &g_leds[3]
};
static std::array<IInstrument*, kNumPads> g_instrument_ptrs = {
    &g_bass_drum,  // PadIndex::Bass      = 0
    &g_snare_drum, // PadIndex::Snare     = 1
    &g_hi_hat,     // PadIndex::HiHat     = 2
    &g_resonator,  // PadIndex::Resonator = 3
};

// DrumMachine borrows everything by reference; instruments must not be Init'd
// yet at construction time (DrumMachine::Init() forwards sample rate + depth).
static DrumMachine g_machine(g_button_ptrs, g_led_ptrs, g_instrument_ptrs, g_rng);

// ---- Audio callback --------------------------------------------------------
// Mirrors the snippet in SPECIFICATION.md §DrumMachine API.
static void AudioCallback(daisy::AudioHandle::InputBuffer  /*in*/,
                          daisy::AudioHandle::OutputBuffer out,
                          size_t                           size) {
    g_machine.Tick(daisy::System::GetNow());
    for (size_t s = 0; s < size; ++s) {
        const float y = g_machine.Process();
        out[0][s] = y;
        out[1][s] = y;
    }
}

// ---- Helpers ---------------------------------------------------------------
// TIM4 channel index for each pad's LED, matching Config::kLedPins:
//   PadIndex::Bass      → D13 → CH1
//   PadIndex::Snare     → D14 → CH2
//   PadIndex::HiHat     → D11 → CH3
//   PadIndex::Resonator → D12 → CH4
// If the hardware integrator reshuffles kLedPins they must update this table
// (and keep all four LED pins on a single TIM peripheral).
static constexpr int kLedChannelIndex[kNumPads] = {1, 2, 3, 4};

static void InitLedPwm() {
    // TIM4 is a 16-bit timer (max period 0xFFFF). With prescaler=0 and
    // period=0xFFFF, the resulting PWM frequency at the Daisy's 200 MHz APB
    // clock is well above audio range, so any switching noise is inaudible.
    daisy::PWMHandle::Config pwm_cfg(
        daisy::PWMHandle::Config::Peripheral::TIM_4,
        /*prescaler=*/0,
        /*period=*/0xFFFF);
    led_pwm.Init(pwm_cfg);

    // Each channel is bound to a fixed Daisy Seed pin (see per/pwm.h table).
    daisy::PWMHandle::Channel::Config ch1_cfg(Config::kLedPins[0]);
    daisy::PWMHandle::Channel::Config ch2_cfg(Config::kLedPins[1]);
    daisy::PWMHandle::Channel::Config ch3_cfg(Config::kLedPins[2]);
    daisy::PWMHandle::Channel::Config ch4_cfg(Config::kLedPins[3]);
    led_pwm.Channel1().Init(ch1_cfg);
    led_pwm.Channel2().Init(ch2_cfg);
    led_pwm.Channel3().Init(ch3_cfg);
    led_pwm.Channel4().Init(ch4_cfg);
}

// ---- main ------------------------------------------------------------------
int main(void) {
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(Config::kAudioBlockSize);
    hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);

    const float sample_rate = hw.AudioSampleRate();

    // Seed the RNG from a microsecond-resolution timer sampled exactly once.
    // Must come AFTER hw.Init() — the system clock that backs GetUs() is
    // configured there. XorShiftRng substitutes a non-zero default if the
    // seed happens to be zero, but we also guard here for clarity.
    const uint32_t seed = daisy::System::GetUs();
    g_rng = XorShiftRng(seed != 0u ? seed : 1u);

    // Buttons: active-low arcade buttons with internal pull-up.
    for (std::size_t i = 0; i < kNumPads; ++i) {
        g_buttons[i].Init(Config::kButtonPins[i], sample_rate);
    }

    // LEDs: shared TIM4 PWMHandle owns the four channels; each DaisyLed binds
    // to one channel of that handle.
    InitLedPwm();
    for (std::size_t i = 0; i < kNumPads; ++i) {
        g_leds[i].Init(led_pwm, kLedChannelIndex[i]);
    }

    // Init the engine — forwards (sample_rate, kDefaultRandomizationDepth) to
    // each instrument and re-applies the boot-stuck mask on every pad.
    g_machine.Init(sample_rate);

    hw.StartAudio(AudioCallback);

    // Idle. All work happens inside the audio callback.
    while (true) {
        // No control code outside the audio path — buttons are sampled at
        // block rate by DrumMachine::Tick(); LEDs are written by DaisyLed.
    }
}
