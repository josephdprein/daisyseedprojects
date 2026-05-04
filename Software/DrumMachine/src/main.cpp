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
//
// ---- Diagnostic instrumentation -------------------------------------------
// This file also hosts the firmware-side implementation of
// drum_machine::diag::Push (see src/util/Diagnostic.h): a lock-free SPSC
// ring buffer the audio callback / engine code pushes events onto, drained
// and printed over USB CDC by the idle main loop. Boot-time facts (sample
// rate, block size, PWM init result, initial button states) are printed
// directly from main(). The on-board user LED is toggled from the audio
// callback as a no-USB-required heartbeat. Remove the diag plumbing once
// the silent-audio bug is identified.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "daisy_seed.h"
#include "per/gpio.h"
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
#include "util/Diagnostic.h"

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

// ---- Diagnostic state ------------------------------------------------------
namespace {

constexpr const char* kPadNames[kNumPads] = {"bass", "snare", "hihat", "resonator"};

inline const char* PadName(uint8_t pad_id) {
    return pad_id < kNumPads ? kPadNames[pad_id] : "?";
}

// SPSC ring buffer. Producer = audio callback (interrupt context); consumer
// = main loop. uint32_t loads/stores on Cortex-M7 are atomic, so plain
// `volatile` is sufficient — no std::atomic needed.
struct DiagRecord {
    uint8_t event;   // drum_machine::diag::PadEvent
    uint8_t pad_id;
};

constexpr size_t kDiagCapacity = 64;  // power of two; mask is faster than %.
static_assert((kDiagCapacity & (kDiagCapacity - 1)) == 0,
              "kDiagCapacity must be a power of two");

static DiagRecord g_diag_buf[kDiagCapacity];
static volatile uint32_t g_diag_head = 0;  // written by producer
static volatile uint32_t g_diag_tail = 0;  // written by consumer

// Audio-callback heartbeat: counter incremented every block, sampled from
// the main loop. Volatile to prevent the optimizer from caching it.
static volatile uint32_t g_audio_block_count = 0;

// Peak |sample| seen in the most recently-completed audio block. Lets us
// distinguish "callback runs but mixer outputs zeros" from "callback runs
// and mixer is producing audio."
static volatile float g_last_block_peak = 0.0f;

// Captured exactly once on the first AudioCallback invocation, so the main
// loop can print what the callback actually saw (vs. what hw.AudioSampleRate
// reported in main()).
static volatile bool   g_first_cb_seen = false;
static volatile size_t g_first_cb_size = 0;

}  // namespace

// ---- diag::Push firmware definition ---------------------------------------
// Declared in src/util/Diagnostic.h; the host build collapses to an inline
// no-op so unit tests don't need this definition.
namespace drum_machine {
namespace diag {

void Push(PadEvent ev, uint8_t pad_id) {
    const uint32_t head = g_diag_head;
    const uint32_t next = (head + 1) & (kDiagCapacity - 1);
    if (next == g_diag_tail) {
        // Buffer full — drop the event rather than block the audio callback.
        // We tolerate this because the consumer is the idle loop and should
        // drain faster than realistic event rates (a few per button press).
        return;
    }
    g_diag_buf[head].event  = static_cast<uint8_t>(ev);
    g_diag_buf[head].pad_id = pad_id;
    // Publish the record before advancing head so the consumer can't observe
    // a half-written slot. ARM data memory ordering + a compiler barrier are
    // enough on this single-core MCU.
    __asm__ volatile("" ::: "memory");
    g_diag_head = next;
}

}  // namespace diag
}  // namespace drum_machine

// ---- Audio callback --------------------------------------------------------
// Mirrors the snippet in SPECIFICATION.md §DrumMachine API, plus a
// callback-running heartbeat (LED toggle + counter) and a per-block peak
// tracker for the diagnostic stream.
static void AudioCallback(daisy::AudioHandle::InputBuffer  /*in*/,
                          daisy::AudioHandle::OutputBuffer out,
                          size_t                           size) {
    // Capture first-callback facts so the main loop can confirm what we saw.
    if (!g_first_cb_seen) {
        g_first_cb_size = size;
        g_first_cb_seen = true;
    }

    g_machine.Tick(daisy::System::GetNow());

    float peak = 0.0f;
    for (size_t s = 0; s < size; ++s) {
        const float y = g_machine.Process();
        out[0][s] = y;
        out[1][s] = y;
        const float a = std::fabs(y);
        if (a > peak) {
            peak = a;
        }
    }
    g_last_block_peak = peak;

    // Visual heartbeat: toggle the on-board user LED every ~256 blocks. At
    // 1 ms per block (block size 48 @ 48 kHz) that's a ~3.9 Hz blink — easy
    // to see by eye, doesn't depend on a USB host being attached.
    const uint32_t count = g_audio_block_count + 1;
    g_audio_block_count = count;
    if ((count & 0xFF) == 0) {
        hw.SetLed((count >> 8) & 1);
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

// Returns true on success. Logs each step so we can see exactly where TIM4
// init breaks if it does.
static bool InitLedPwm() {
    // TIM4 is a 16-bit timer (max period 0xFFFF). With prescaler=0 and
    // period=0xFFFF, the resulting PWM frequency at the Daisy's 200 MHz APB
    // clock is well above audio range, so any switching noise is inaudible.
    daisy::PWMHandle::Config pwm_cfg(
        daisy::PWMHandle::Config::Peripheral::TIM_4,
        /*prescaler=*/0,
        /*period=*/0xFFFF);
    const auto periph_res = led_pwm.Init(pwm_cfg);
    hw.PrintLine("[BOOT] PWM TIM4 init=%s",
                 periph_res == daisy::PWMHandle::Result::OK ? "OK" : "ERR");
    if (periph_res != daisy::PWMHandle::Result::OK) {
        return false;
    }

    // Each channel is bound to a fixed Daisy Seed pin (see per/pwm.h table).
    daisy::PWMHandle::Channel::Config ch_cfgs[kNumPads] = {
        daisy::PWMHandle::Channel::Config(Config::kLedPins[0]),
        daisy::PWMHandle::Channel::Config(Config::kLedPins[1]),
        daisy::PWMHandle::Channel::Config(Config::kLedPins[2]),
        daisy::PWMHandle::Channel::Config(Config::kLedPins[3]),
    };
    daisy::PWMHandle::Channel* channels[kNumPads] = {
        &led_pwm.Channel1(), &led_pwm.Channel2(),
        &led_pwm.Channel3(), &led_pwm.Channel4(),
    };

    bool ok = true;
    for (size_t i = 0; i < kNumPads; ++i) {
        const auto r = channels[i]->Init(ch_cfgs[i]);
        hw.PrintLine("[BOOT]   ch%u (%s) init=%s",
                     (unsigned)(i + 1), kPadNames[i],
                     r == daisy::PWMHandle::Result::OK ? "OK" : "ERR");
        if (r != daisy::PWMHandle::Result::OK) {
            ok = false;
        }
    }
    return ok;
}

// Quick raw-GPIO read of each button pin BEFORE DaisyButton init takes over.
// Enables the same internal pull-up DaisyButton uses, so an open switch
// reads HIGH ("up") and a switch closed to GND reads LOW ("DOWN").
//
// We DeInit() each probe before letting DaisyButton::Init() reconfigure the
// pin. The raw read confirms the wiring: with one button on D7 to GND, the
// log should show D7 toggling DOWN/up depending on the button's state at
// reset, and the other three pins should always read up (idle pull-up).
static void LogInitialButtonStates() {
    hw.PrintLine("[BOOT] initial button states (after pull-up settles):");
    for (size_t i = 0; i < kNumPads; ++i) {
        daisy::GPIO probe;
        probe.Init(Config::kButtonPins[i],
                   daisy::GPIO::Mode::INPUT,
                   daisy::GPIO::Pull::PULLUP);
        // Allow ~1 ms for the internal pull-up to settle a floating line.
        daisy::System::Delay(1);
        const bool raw_high = probe.Read();
        hw.PrintLine("[BOOT]   pad %u (%s): %s",
                     (unsigned)i, kPadNames[i],
                     raw_high ? "up" : "DOWN");
        probe.DeInit();
    }
}

static void DrainDiagLog() {
    while (g_diag_tail != g_diag_head) {
        const DiagRecord r = g_diag_buf[g_diag_tail];
        switch (static_cast<drum_machine::diag::PadEvent>(r.event)) {
            case drum_machine::diag::EvPress:
                hw.PrintLine("[PAD%u %s] press", r.pad_id, PadName(r.pad_id));
                break;
            case drum_machine::diag::EvRelease:
                hw.PrintLine("[PAD%u %s] release", r.pad_id, PadName(r.pad_id));
                break;
            case drum_machine::diag::EvTrig:
                hw.PrintLine("[TRIG %s]", PadName(r.pad_id));
                break;
            case drum_machine::diag::EvBootMaskEngaged:
                hw.PrintLine("[PAD%u %s] boot-stuck mask engaged",
                             r.pad_id, PadName(r.pad_id));
                break;
            case drum_machine::diag::EvBootMaskCleared:
                hw.PrintLine("[PAD%u %s] boot-stuck mask cleared",
                             r.pad_id, PadName(r.pad_id));
                break;
        }
        // Single-byte advance is atomic on M7; no barrier needed because
        // there's only one consumer.
        g_diag_tail = (g_diag_tail + 1) & (kDiagCapacity - 1);
    }
}

// ---- main ------------------------------------------------------------------
int main(void) {
    hw.Configure();
    hw.Init();
    // Non-blocking USB CDC log — boots even if no host is attached so the
    // firmware doesn't hang waiting for `dfu-util` to finish releasing the
    // USB or for a serial monitor to connect.
    hw.StartLog(/*wait_for_pc=*/false);

    hw.SetAudioBlockSize(Config::kAudioBlockSize);
    hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);

    const float  sample_rate = hw.AudioSampleRate();
    const size_t block_size  = hw.AudioBlockSize();
    hw.PrintLine("[BOOT] DrumMachine firmware starting");
    hw.PrintLine("[BOOT] sample_rate=%d Hz block_size=%u",
                 (int)sample_rate, (unsigned)block_size);

    // Seed the RNG from a microsecond-resolution timer sampled exactly once.
    // Must come AFTER hw.Init() — the system clock that backs GetUs() is
    // configured there. XorShiftRng substitutes a non-zero default if the
    // seed happens to be zero, but we also guard here for clarity.
    const uint32_t seed = daisy::System::GetUs();
    g_rng = XorShiftRng(seed != 0u ? seed : 1u);
    hw.PrintLine("[BOOT] rng seed=%lu", (unsigned long)seed);

    // Sample raw GPIO state of each button BEFORE DaisyButton::Init grabs
    // the pin. Helps catch wiring mistakes (button to 3V3, swapped pins,
    // stuck-low solder bridges) and lets us correlate the boot-stuck mask.
    LogInitialButtonStates();

    // Buttons: active-low arcade buttons with internal pull-up.
    for (std::size_t i = 0; i < kNumPads; ++i) {
        g_buttons[i].Init(Config::kButtonPins[i], sample_rate);
    }

    // LEDs: shared TIM4 PWMHandle owns the four channels; each DaisyLed binds
    // to one channel of that handle.
    const bool pwm_ok = InitLedPwm();
    (void)pwm_ok;  // Logged inside; we still bind LEDs even on failure so a
                   // partial PWM init doesn't break the engine.
    for (std::size_t i = 0; i < kNumPads; ++i) {
        g_leds[i].Init(led_pwm, kLedChannelIndex[i]);
    }

    // Init the engine — forwards (sample_rate, kDefaultRandomizationDepth) to
    // each instrument and re-applies the boot-stuck mask on every pad.
    g_machine.Init(sample_rate);
    hw.PrintLine("[BOOT] engine initialized; starting audio");

    hw.StartAudio(AudioCallback);
    hw.PrintLine("[BOOT] StartAudio returned; entering idle loop");

    // ---- Idle loop: drain the diagnostic ring buffer and emit periodic
    // heartbeats. Per spec, no control logic runs here — buttons are sampled
    // at block rate by DrumMachine::Tick(); LEDs are written by DaisyLed.
    uint32_t last_heartbeat_ms = daisy::System::GetNow();
    uint32_t last_block_count  = 0;
    bool     printed_first_cb  = false;

    while (true) {
        DrainDiagLog();

        // Print the first-callback facts as soon as we observe the flag flip.
        if (!printed_first_cb && g_first_cb_seen) {
            hw.PrintLine("[CB] first callback fired: size=%u",
                         (unsigned)g_first_cb_size);
            printed_first_cb = true;
        }

        // 1 Hz heartbeat: callback count delta + latest block peak. If the
        // delta stays at 0 the audio callback is not running — even though
        // hw.StartAudio returned, the SAI/codec stream isn't producing
        // blocks. If the delta is healthy but peak stays 0.0, the callback
        // runs but the mixer is silent (instruments not producing samples).
        const uint32_t now_ms = daisy::System::GetNow();
        if (now_ms - last_heartbeat_ms >= 1000u) {
            const uint32_t cur = g_audio_block_count;
            const uint32_t delta = cur - last_block_count;
            // Cast peak to int (×1000) since PrintLine doesn't reliably
            // format floats on all libDaisy log backends.
            const int peak_milli = (int)(g_last_block_peak * 1000.0f);
            hw.PrintLine("[CB] count=%lu delta=%lu peak_x1000=%d",
                         (unsigned long)cur, (unsigned long)delta, peak_milli);
            last_block_count   = cur;
            last_heartbeat_ms  = now_ms;
        }
    }
}
